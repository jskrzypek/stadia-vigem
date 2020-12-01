#include <tchar.h>
#include <windows.h>
#include <synchapi.h>

#include "stadia.h"
#include "hid.h"

#pragma comment(lib, "kernel32.lib")

#define STADIA_READ_TIMEOUT 10

#define STADIA_VIBRATION_HEADER         0
#define STADIA_VIBRATION_BIG_MOTOR      2
#define STADIA_VIBRATION_SMALL_MOTOR    4

static const BYTE init_vibration[5] = { 0x05, 0x00, 0x00, 0x00, 0x00 };

static const DWORD dpad_map[8] =
{
    STADIA_BUTTON_UP,
    STADIA_BUTTON_UP | STADIA_BUTTON_RIGHT,
    STADIA_BUTTON_RIGHT,
    STADIA_BUTTON_RIGHT | STADIA_BUTTON_DOWN,
    STADIA_BUTTON_DOWN,
    STADIA_BUTTON_DOWN | STADIA_BUTTON_LEFT,
    STADIA_BUTTON_LEFT,
    STADIA_BUTTON_LEFT | STADIA_BUTTON_UP
};

struct stadia_controller;

struct stadia_controller
{
    int id;
    struct hid_device *device;
    SRWLOCK state_lock;
    struct stadia_state state;
    BOOL hold_stadia_btn;
    void (*upd_cb)(int, struct stadia_state *);
    void (*stop_cb)(int, BYTE);

    BOOL active;
    HANDLE stopping_event;

    HANDLE in_thread;
    HANDLE out_event;
    HANDLE out_thread;
    HANDLE delay_thread;
    HANDLE delay_event;

    SRWLOCK vibr_lock;
    BYTE small_motor;
    BYTE big_motor;

    struct stadia_controller *prev;
    struct stadia_controller *next;
};

static int last_controller_id = 0;
static struct stadia_controller *root_controller = NULL;
static SRWLOCK controller_lock = SRWLOCK_INIT;

static DWORD WINAPI _stadia_input_thread_proc(LPVOID lparam)
{
    struct stadia_controller *controller = (struct stadia_controller *)lparam;
    INT bytes_read = 0;
    BYTE break_reason = STADIA_BREAK_REASON_UNKNOWN;

    while (TRUE)
    {
        while (controller->active && (bytes_read = hid_get_input_report(controller->device, STADIA_READ_TIMEOUT)) == 0)
        {
            ;
        }

        if (!controller->active || bytes_read < 0)
        {
            break_reason = !controller->active ? STADIA_BREAK_REASON_REQUESTED : STADIA_BREAK_REASON_READ_ERROR;
            break;
        }

        // check packet header
        if (controller->device->input_buffer[0] != 0x03)
        {
            continue;
        }

        AcquireSRWLockExclusive(&controller->state_lock);

        controller->state.buttons = STADIA_BUTTON_NONE;

        controller->state.buttons |= controller->device->input_buffer[1] < 8 ? dpad_map[controller->device->input_buffer[1]] : 0;

        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 7)) != 0 ? STADIA_BUTTON_RS : 0;
        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 6)) != 0 ? STADIA_BUTTON_OPTIONS : 0;
        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 5)) != 0 ? STADIA_BUTTON_MENU : 0;
        controller->state.buttons |= (controller->device->input_buffer[2] & (1 << 4)) != 0 ? STADIA_BUTTON_STADIA_BTN : 0;

        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 6)) != 0 ? STADIA_BUTTON_A : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 5)) != 0 ? STADIA_BUTTON_B : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 4)) != 0 ? STADIA_BUTTON_X : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 3)) != 0 ? STADIA_BUTTON_Y : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 2)) != 0 ? STADIA_BUTTON_L1 : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 1)) != 0 ? STADIA_BUTTON_R1 : 0;
        controller->state.buttons |= (controller->device->input_buffer[3] & (1 << 0)) != 0 ? STADIA_BUTTON_LS : 0;

        controller->state.left_stick_x = controller->device->input_buffer[4];
        controller->state.left_stick_y = controller->device->input_buffer[5];
        controller->state.right_stick_x = controller->device->input_buffer[6];
        controller->state.right_stick_y = controller->device->input_buffer[7];

        controller->state.l2_trigger = controller->device->input_buffer[8];
        controller->state.r2_trigger = controller->device->input_buffer[9];

        controller->state.battery = controller->device->input_buffer[10];

        ReleaseSRWLockExclusive(&controller->state_lock);

        controller->upd_cb(controller->id, &controller->state);
    }

    controller->active = FALSE;
    SetEvent(controller->stopping_event);
    HANDLE wait_threads[2] = { controller->out_thread, controller->delay_thread };
    WaitForMultipleObjects(2, wait_threads, TRUE, INFINITE);

    AcquireSRWLockExclusive(&controller_lock);
    if (controller->prev == NULL)
    {
        root_controller = controller->next;
    }
    else
    {
        controller->prev->next = controller->next;
        if (controller->next != NULL)
        {
            controller->next->prev = controller->prev;
        }
    }
    ReleaseSRWLockExclusive(&controller_lock);

    CloseHandle(controller->stopping_event);
    CloseHandle(controller->out_event);
    CloseHandle(controller->delay_event);
    CloseHandle(controller->in_thread);
    CloseHandle(controller->out_thread);
    CloseHandle(controller->delay_thread);
    controller->stop_cb(controller->id, break_reason);
    free(controller);

    return 0;
}

static DWORD WINAPI _stadia_output_thread_proc(LPVOID lparam)
{
    struct stadia_controller *controller = (struct stadia_controller *)lparam;
    
    HANDLE dummy_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE wait_events[3] = { controller->stopping_event, controller->out_event, dummy_event };
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    LARGE_INTEGER due_time;
    due_time.QuadPart = -20000000LL; // 2 sec
    BYTE vibration[5] = {0};
    vibration[STADIA_VIBRATION_HEADER] = init_vibration[STADIA_VIBRATION_HEADER];
    vibration[STADIA_VIBRATION_BIG_MOTOR] = controller->big_motor;
    vibration[STADIA_VIBRATION_SMALL_MOTOR] = controller->small_motor;

    while (controller->active)
    {
        DWORD wait_result = WaitForMultipleObjects(3, wait_events, FALSE, INFINITE);
        // if timer signaled
        if (wait_result == WAIT_OBJECT_0 + 2)
        {
            AcquireSRWLockExclusive(&controller->vibr_lock);
            controller->small_motor = 0;
            controller->big_motor = 0;
            ReleaseSRWLockExclusive(&controller->vibr_lock);
            wait_events[2] = dummy_event;
        }

        AcquireSRWLockShared(&controller->vibr_lock);
        if (controller->big_motor != vibration[STADIA_VIBRATION_BIG_MOTOR] || controller->small_motor != vibration[STADIA_VIBRATION_SMALL_MOTOR])
        {
            vibration[STADIA_VIBRATION_BIG_MOTOR] = controller->big_motor;
            vibration[STADIA_VIBRATION_SMALL_MOTOR] = controller->small_motor;

            if (controller->small_motor != 0 || controller->big_motor != 0)
            {
                SetWaitableTimer(timer, &due_time, 0, NULL, NULL, FALSE);
                wait_events[2] = timer;
            }
            else if (wait_events[2] == timer)
            {
                CancelWaitableTimer(timer);
                wait_events[2] = dummy_event;
            }

            ReleaseSRWLockShared(&controller->vibr_lock);
            hid_send_output_report(controller->device, vibration, sizeof(vibration));
        }
        else
        {
            ReleaseSRWLockShared(&controller->vibr_lock);
        }
        ResetEvent(controller->out_event);
    }

    // turn off vibration
    hid_send_output_report(controller->device, init_vibration, sizeof(init_vibration));

    CloseHandle(dummy_event);
    CloseHandle(timer);

    return 0;
}

static DWORD WINAPI _stadia_delay_thread_proc(LPVOID lparam)
{
    struct stadia_controller *controller = (struct stadia_controller *)lparam;

    HANDLE wait_events[2] = { controller->stopping_event, controller->delay_event };
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    HANDLE wait_delay_objects[2] = { controller->stopping_event, timer };
    LARGE_INTEGER due_time;
    due_time.QuadPart = -2000000LL; // 200 msec

    while (controller->active)
    {
        WaitForMultipleObjects(2, wait_events, FALSE, INFINITE);
        if (controller->hold_stadia_btn)
        {
            SetWaitableTimer(timer, &due_time, 0, NULL, NULL, FALSE);
            WaitForMultipleObjects(2, wait_delay_objects, FALSE, INFINITE);

            AcquireSRWLockExclusive(&controller->state_lock);
            controller->hold_stadia_btn = FALSE;
            controller->state.buttons ^= STADIA_BUTTON_STADIA_BTN;
            ReleaseSRWLockExclusive(&controller->state_lock);

            controller->upd_cb(controller->id, &controller->state);
        }
        ResetEvent(controller->delay_event);
    }

    CloseHandle(timer);

    return 0;
}

int stadia_controller_start(struct hid_device *device, void (*upd_cb)(int, struct stadia_state *), void (*stop_cb)(int, BYTE))
{
    if (hid_send_output_report(device, init_vibration, sizeof(init_vibration)) <= 0)
    {
        return -1;
    }

    SECURITY_ATTRIBUTES security =
    {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = NULL,
        .bInheritHandle = TRUE
    };

    struct stadia_controller *controller = (struct stadia_controller *)malloc(sizeof(struct stadia_controller));
    controller->id = ++last_controller_id;
    controller->device = device;
    InitializeSRWLock(&controller->state_lock);
    controller->hold_stadia_btn = FALSE;
    controller->upd_cb = upd_cb;
    controller->stop_cb = stop_cb;
    controller->active = TRUE;
    controller->stopping_event = CreateEvent(&security, TRUE, FALSE, NULL);
    controller->out_event = CreateEvent(&security, TRUE, FALSE, NULL);
    controller->delay_event = CreateEvent(&security, TRUE, FALSE, NULL);
    InitializeSRWLock(&controller->vibr_lock);
    controller->small_motor = init_vibration[STADIA_VIBRATION_SMALL_MOTOR];
    controller->big_motor = init_vibration[STADIA_VIBRATION_BIG_MOTOR];
    controller->next = NULL;

    controller->in_thread = CreateThread(&security, 0, _stadia_input_thread_proc, controller, CREATE_SUSPENDED, NULL);
    controller->out_thread = CreateThread(&security, 0, _stadia_output_thread_proc, controller, CREATE_SUSPENDED, NULL);
    controller->delay_thread = CreateThread(&security, 0, _stadia_delay_thread_proc, controller, CREATE_SUSPENDED, NULL);
    if (controller->in_thread == NULL || controller->out_thread == NULL || controller->delay_thread == NULL)
    {
        if (controller->in_thread != NULL)
        {
            CloseHandle(controller->in_thread);
        }
        if (controller->out_thread != NULL)
        {
            CloseHandle(controller->out_thread);
        }
        if (controller->delay_thread != NULL)
        {
            CloseHandle(controller->delay_thread);
        }
        free(controller);
        return -1;
    }

    AcquireSRWLockExclusive(&controller_lock);
    if (root_controller == NULL)
    {
        root_controller = controller;
        controller->prev = NULL;
    }
    else
    {
        struct stadia_controller *cur_controller = root_controller;
        while (cur_controller->next != NULL)
        {
            cur_controller = cur_controller->next;
        }
        cur_controller->next = controller;
        controller->prev = cur_controller;
    }
    ReleaseSRWLockExclusive(&controller_lock);

    ResumeThread(controller->in_thread);
    ResumeThread(controller->out_thread);
    ResumeThread(controller->delay_thread);
    return controller->id;
}

void stadia_controller_set_vibration(int controller_id, BYTE small_motor, BYTE big_motor)
{
    AcquireSRWLockShared(&controller_lock);
    struct stadia_controller *cur_controller = root_controller;
    while (cur_controller != NULL)
    {
        if (cur_controller->id == controller_id)
        {
            break;
        }
        cur_controller = cur_controller->next;
    }
    ReleaseSRWLockShared(&controller_lock);
    if (cur_controller != NULL)
    {
        AcquireSRWLockExclusive(&cur_controller->vibr_lock);
        cur_controller->small_motor = small_motor;
        cur_controller->big_motor = big_motor;
        ReleaseSRWLockExclusive(&cur_controller->vibr_lock);
        SetEvent(cur_controller->out_event);
    }
}

void stadia_controller_stop(int controller_id)
{
    AcquireSRWLockShared(&controller_lock);
    struct stadia_controller *cur_controller = root_controller;
    while (cur_controller != NULL)
    {
        if (cur_controller->id == controller_id)
        {
            cur_controller->active = FALSE;
            CancelIoEx(cur_controller->device->handle, &cur_controller->device->input_ol);
            break;
        }
        cur_controller = cur_controller->next;
    }
    ReleaseSRWLockShared(&controller_lock);
}
