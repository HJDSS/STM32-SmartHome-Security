#include "capture_task.h"

volatile u8 g_cap_evt_pending;

void Capture_Task_Create(void)
{
    g_cap_evt_pending = 0;
}

void Capture_Request(CAP_EVT_T evt)
{
    (void)evt;
    g_cap_evt_pending = 1u;
}
