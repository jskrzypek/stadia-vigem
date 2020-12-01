#ifndef STADIA_H
#define STADIA_H

#include <wtypes.h>

#define STADIA_HW_VENDOR_ID                 0x18D1
#define STADIA_HW_PRODUCT_ID                0x9400

#define STADIA_HW_FILTER                    TEXT("VID_18D1&PID_9400")

#define STADIA_BUTTON_NONE                  0x00000000
#define STADIA_BUTTON_A                     0x00000001
#define STADIA_BUTTON_B                     0x00000002
#define STADIA_BUTTON_X                     0x00000004
#define STADIA_BUTTON_Y                     0x00000008
#define STADIA_BUTTON_L1                    0x00000010
#define STADIA_BUTTON_R1                    0x00000020
#define STADIA_BUTTON_LS                    0x00000040
#define STADIA_BUTTON_RS                    0x00000080
#define STADIA_BUTTON_UP                    0x00000100
#define STADIA_BUTTON_DOWN                  0x00000200
#define STADIA_BUTTON_LEFT                  0x00000400
#define STADIA_BUTTON_RIGHT                 0x00000800
#define STADIA_BUTTON_OPTIONS               0x00001000
#define STADIA_BUTTON_MENU                  0x00002000
#define STADIA_BUTTON_STADIA_BTN            0x00004000

#define STADIA_BREAK_REASON_UNKNOWN         0x0000
#define STADIA_BREAK_REASON_REQUESTED       0x0001
#define STADIA_BREAK_REASON_INIT_ERROR      0x0002
#define STADIA_BREAK_REASON_READ_ERROR      0x0004
#define STADIA_BREAK_REASON_WRITE_ERROR     0x0008

struct stadia_state
{
    DWORD buttons;
    
    BYTE left_stick_x;
    BYTE left_stick_y;
    BYTE right_stick_x;
    BYTE right_stick_y;
    
    BYTE l2_trigger;
    BYTE r2_trigger;

    BYTE battery;
};

int stadia_controller_start(struct hid_device *device, void (*upd_cb)(int, struct stadia_state *), void (*stop_cb)(int, BYTE));
void stadia_controller_set_vibration(int controller_id, BYTE small_motor, BYTE big_motor);
void stadia_controller_stop(int controller_id);

#endif /* STADIA_H */
