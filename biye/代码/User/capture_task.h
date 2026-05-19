#ifndef __CAPTURE_TASK_H
#define __CAPTURE_TASK_H

#include "sys.h"

typedef enum
{
    CAP_EVT_INTRUSION = 1,
    CAP_EVT_PWD_LOCK  = 2,
    CAP_EVT_GAS       = 3,
    CAP_EVT_REMOTE    = 4,  /* 🟡2: 远程手动抓拍命令 */
}CAP_EVT_T;

extern volatile u8 g_cap_evt_pending;

void Capture_Task_Create(void);
void Capture_Request(CAP_EVT_T evt);

void Capture_OnSaved(const char *filename);

#endif
