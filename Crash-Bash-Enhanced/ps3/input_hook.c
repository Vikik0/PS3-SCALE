#include "input_hook.h"

#include <stddef.h>
#include <stdint.h>


static void *cb_memset(void *destination, int value, size_t length)
{
    uint8_t *out = (uint8_t *)destination;
    size_t i;
    for (i = 0; i < length; ++i) out[i] = (uint8_t)value;
    return destination;
}

typedef int32_t (*CellPadGetDataFn)(uint32_t port_no, CellPadData *data);
typedef int32_t (*CellPadGetInfo2Fn)(CellPadInfo2 *info);
typedef int32_t (*CellPadGetDataExtraFn)(uint32_t port_no, uint32_t *device_type, CellPadData *data);

typedef struct {
    uint32_t entry;
    uint32_t toc;
} CbFunctionDescriptor;

static CbFunctionDescriptor original_get_data_descriptor;
static CbFunctionDescriptor original_get_info2_descriptor;
static CbFunctionDescriptor original_get_data_extra_descriptor;
static CellPadGetDataFn original_get_data =
    (CellPadGetDataFn)(void *)&original_get_data_descriptor;
static CellPadGetInfo2Fn original_get_info2 =
    (CellPadGetInfo2Fn)(void *)&original_get_info2_descriptor;
static CellPadGetDataExtraFn original_get_data_extra =
    (CellPadGetDataExtraFn)(void *)&original_get_data_extra_descriptor;

typedef struct {
    volatile uint32_t sequence;
    CbPadState state;
} CbRemoteSlot;

static CbRemoteSlot remote_slots[4];
static volatile int bridge_active;
static volatile int local_player_slot = 0;
static int hooks_installed;

static int descriptor_patch(void *target, void *replacement, CbFunctionDescriptor *saved)
{
    uint32_t *target_words = (uint32_t *)target;
    const uint32_t *replacement_words = (const uint32_t *)replacement;

    if (!target || !replacement || !saved) {
        return -1;
    }

    saved->entry = target_words[0];
    saved->toc = target_words[1];

    if ((((uintptr_t)target | (uintptr_t)replacement) & 7u) == 0) {
        *(volatile uint64_t *)target = *(const uint64_t *)replacement;
    } else {
        target_words[0] = replacement_words[0];
        target_words[1] = replacement_words[1];
    }

    __asm__ volatile("sync" ::: "memory");
    return 0;
}

static void descriptor_restore(void *target, const CbFunctionDescriptor *saved)
{
    uint32_t *words = (uint32_t *)target;

    if (!target || !saved) {
        return;
    }

    if (((uintptr_t)target & 7u) == 0) {
        uint64_t both = ((uint64_t)saved->entry << 32) | saved->toc;
        *(volatile uint64_t *)target = both;
    } else {
        words[0] = saved->entry;
        words[1] = saved->toc;
    }

    __asm__ volatile("sync" ::: "memory");
}

static CbPadState remote_snapshot(int slot)
{
    CbPadState copy;
    uint32_t before;
    uint32_t after;

    cb_memset(&copy, 0, sizeof(copy));
    copy.right_x = 0x80;
    copy.right_y = 0x80;
    copy.left_x = 0x80;
    copy.left_y = 0x80;

    for (;;) {
        before = remote_slots[slot].sequence;
        if (before & 1u) {
            continue;
        }
        __asm__ volatile("sync" ::: "memory");
        copy = remote_slots[slot].state;
        __asm__ volatile("sync" ::: "memory");
        after = remote_slots[slot].sequence;
        if (before == after && !(after & 1u)) {
            break;
        }
    }

    return copy;
}

static void state_to_report(const CbPadState *state, CellPadData *data)
{
    cb_memset(data, 0, sizeof(*data));
    data->len = 8;
    data->button[2] = state->digital1;
    data->button[3] = state->digital2;
    data->button[4] = state->right_x;
    data->button[5] = state->right_y;
    data->button[6] = state->left_x;
    data->button[7] = state->left_y;
}

static int32_t cb_cell_pad_get_data(uint32_t port_no, CellPadData *data)
{
    CbPadState state;
    int slot;

    if (!bridge_active || port_no >= 4 || !data) {
        return original_get_data(port_no, data);
    }

    slot = (int)port_no;
    if (slot == local_player_slot) {
        /* The one physical controller remains PS3 port 0, regardless of
         * which virtual PS1 player slot the room assigned to this console. */
        return original_get_data(0, data);
    }

    state = remote_snapshot(slot);
    state_to_report(&state, data);
    return CELL_PAD_OK;
}


static int32_t cb_cell_pad_get_data_extra(uint32_t port_no, uint32_t *device_type, CellPadData *data)
{
    CbPadState state;
    int slot;

    if (!bridge_active || port_no >= 4 || !data) {
        return original_get_data_extra(port_no, device_type, data);
    }

    slot = (int)port_no;
    if (slot == local_player_slot) {
        return original_get_data_extra(0, device_type, data);
    }

    state = remote_snapshot(slot);
    state_to_report(&state, data);
    if (device_type) {
        *device_type = CELL_PAD_DEV_TYPE_STANDARD;
    }
    return CELL_PAD_OK;
}

static int32_t cb_cell_pad_get_info2(CellPadInfo2 *info)
{
    int32_t result;
    uint32_t slot;

    if (!info) {
        return CELL_PAD_ERROR_INVALID_PARAMETER;
    }

    result = original_get_info2(info);
    if (result != CELL_PAD_OK || !bridge_active) {
        return result;
    }

    /* Expose four PS1 controller ports to the emulator. Remote ports are
     * backed by injected reports in cb_cell_pad_get_data(). */
    if (info->max_connect < 4) {
        info->max_connect = 4;
    }
    if (info->now_connect < 4) {
        info->now_connect = 4;
    }

    for (slot = 0; slot < 4; ++slot) {
        info->port_status[slot] |= CELL_PAD_STATUS_CONNECTED;
        info->device_type[slot] = CELL_PAD_DEV_TYPE_STANDARD;
        info->device_capability[slot] |= CELL_PAD_CAPABILITY_PS3_CONFORMITY;
    }

    return result;
}

int cb_input_install(void)
{
    if (hooks_installed) {
        return 0;
    }

    if (descriptor_patch((void *)cellPadGetData, (void *)cb_cell_pad_get_data,
                         &original_get_data_descriptor) != 0) {
        return -1;
    }

    if (descriptor_patch((void *)cellPadGetDataExtra, (void *)cb_cell_pad_get_data_extra,
                         &original_get_data_extra_descriptor) != 0) {
        descriptor_restore((void *)cellPadGetData, &original_get_data_descriptor);
        return -1;
    }

    if (descriptor_patch((void *)cellPadGetInfo2, (void *)cb_cell_pad_get_info2,
                         &original_get_info2_descriptor) != 0) {
        descriptor_restore((void *)cellPadGetDataExtra, &original_get_data_extra_descriptor);
        descriptor_restore((void *)cellPadGetData, &original_get_data_descriptor);
        return -1;
    }

    hooks_installed = 1;
    return 0;
}

void cb_input_remove(void)
{
    bridge_active = 0;
    if (!hooks_installed) {
        return;
    }

    descriptor_restore((void *)cellPadGetInfo2, &original_get_info2_descriptor);
    descriptor_restore((void *)cellPadGetDataExtra, &original_get_data_extra_descriptor);
    descriptor_restore((void *)cellPadGetData, &original_get_data_descriptor);
    hooks_installed = 0;
}

void cb_input_set_active(int active, int local_slot)
{
    uint32_t slot;

    if (local_slot < 0 || local_slot > 3) {
        return;
    }

    bridge_active = 0;
    __asm__ volatile("sync" ::: "memory");
    local_player_slot = local_slot;
    for (slot = 0; slot < 4; ++slot) {
        if ((int)slot == local_slot) {
            continue;
        }
        remote_slots[slot].sequence = 0;
        cb_memset(&remote_slots[slot].state, 0, sizeof(remote_slots[slot].state));
        remote_slots[slot].state.right_x = 0x80;
        remote_slots[slot].state.right_y = 0x80;
        remote_slots[slot].state.left_x = 0x80;
        remote_slots[slot].state.left_y = 0x80;
    }

    __asm__ volatile("sync" ::: "memory");
    bridge_active = active ? 1 : 0;
}

void cb_input_set_remote_state(int slot, const CbPadState *state)
{
    CbRemoteSlot *target;

    if (!state || slot < 0 || slot > 3 || slot == local_player_slot) {
        return;
    }

    target = &remote_slots[slot];
    target->sequence++;
    __asm__ volatile("sync" ::: "memory");
    target->state = *state;
    __asm__ volatile("sync" ::: "memory");
    target->sequence++;
}
