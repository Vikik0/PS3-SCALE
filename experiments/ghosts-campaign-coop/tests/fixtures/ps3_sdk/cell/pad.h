#ifndef GHC_TEST_CELL_PAD_H
#define GHC_TEST_CELL_PAD_H

#include <stdint.h>

#define CELL_PAD_MAX_PORT_NUM 7u
#define CELL_PAD_MAX_CODES 64u
#define CELL_PAD_STATUS_CONNECTED (1u << 0)
#define CELL_PAD_OK 0
#define CELL_PAD_ERROR_NO_DEVICE 0x80121107

#define CELL_PAD_BTN_OFFSET_DIGITAL1 2u
#define CELL_PAD_BTN_OFFSET_DIGITAL2 3u
#define CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X 4u
#define CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y 5u
#define CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X 6u
#define CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y 7u
#define CELL_PAD_BTN_OFFSET_PRESS_L2 18u
#define CELL_PAD_BTN_OFFSET_PRESS_R2 19u

typedef struct CellPadInfo2 {
    uint32_t max_connect;
    uint32_t now_connect;
    uint32_t system_info;
    uint32_t port_status[CELL_PAD_MAX_PORT_NUM];
    uint32_t port_setting[CELL_PAD_MAX_PORT_NUM];
    uint32_t device_capability[CELL_PAD_MAX_PORT_NUM];
    uint32_t device_type[CELL_PAD_MAX_PORT_NUM];
} CellPadInfo2;

typedef struct CellPadData {
    int32_t len;
    uint16_t button[CELL_PAD_MAX_CODES];
} CellPadData;

int32_t cellPadGetInfo2(CellPadInfo2 *info);
int32_t cellPadGetData(uint32_t port_no, CellPadData *data);

#endif
