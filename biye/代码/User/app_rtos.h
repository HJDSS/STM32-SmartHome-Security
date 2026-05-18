#ifndef __APP_RTOS_H
#define __APP_RTOS_H

#include "board_config.h"

#if USE_FREERTOS

#include "FreeRTOS.h"
#include "semphr.h"

void AppRTOS_Start(void);

/* IPC: ISR→Task 通知（PIR 二进制信号量） */
void IPC_NotifyPIR_FromISR(BaseType_t *pxHigherPriorityTaskWoken);

/* IPC: EventGroup 事件通知（供 main.c 跨文件调用） */
void IPC_NotifyArmStateChange(void);
void IPC_NotifyNetOnline(void);
void IPC_NotifyCaptureReq(void);

#endif

#endif
