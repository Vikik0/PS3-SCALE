#include "local_input.h"

#include <cell/pad.h>
#include <string.h>

int ghc_local_input_poll_ps3(GhcLocalInputSnapshot *snapshot)
{
    CellPadInfo2 info;
    uint32_t port;

    if (!snapshot) return GHC_LOCAL_INPUT_ERROR_ARGUMENT;

    ghc_local_input_snapshot_clear(snapshot);
    memset(&info, 0, sizeof(info));

    /* The title owns sys_io/cellPad initialization and shutdown. */
    if (cellPadGetInfo2(&info) != CELL_PAD_OK) {
        return GHC_LOCAL_INPUT_ERROR_PAD_INFO;
    }

    for (port = 0; port < GHC_LOCAL_INPUT_PLAYER_COUNT; ++port) {
        CellPadData data;
        int decode_result;

        if ((info.port_status[port] & CELL_PAD_STATUS_CONNECTED) == 0u) continue;

        memset(&data, 0, sizeof(data));
        if (cellPadGetData(port, &data) != CELL_PAD_OK) {
            snapshot->read_error_mask |= (uint8_t)(1u << port);
            continue;
        }

        decode_result = ghc_local_input_decode_port(
            (uint8_t)port, 1u, data.len, data.button, &snapshot->players[port]
        );
        if (decode_result != GHC_LOCAL_INPUT_OK) {
            snapshot->read_error_mask |= (uint8_t)(1u << port);
            continue;
        }

        snapshot->connected_mask |= (uint8_t)(1u << port);
        snapshot->connected_count++;
    }

    return GHC_LOCAL_INPUT_OK;
}
