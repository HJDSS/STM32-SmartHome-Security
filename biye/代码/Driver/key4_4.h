#ifndef __KEY4_4_H
#define __KEY4_4_H	 

#include <stm32f10x.h>
#include "sys.h"


#define uint unsigned int 
#define uchar unsigned char

typedef enum {
	KEY_EVENT_NONE = 0,
	KEY_EVENT_CLICK,
	KEY_EVENT_DOUBLE_CLICK,
	KEY_EVENT_TRIPLE_CLICK,
	KEY_EVENT_LONG_PRESS
} KeyEvent;

void Key_Init(void);
int Key_Scan(void);
void Key_Test(void) ;
u8 Key_Debug_ReadMask(void);
KeyEvent Key_GetLastEvent(int *key_code);

/* 中断/任务协作：空闲时由行线 EXTI 唤醒，非轮询忙等 */
void Key_SetNotifyTask(void *task_handle);
int Key_NeedService(void);
int Key_IsActiveScan(void);
/* 强制触发一次服务（用于从耗时菜单/外设操作返回后确保键盘恢复） */
void Key_ForceService(void);
void Key_Debug_GetCounters(u32 *irq_cnt, u32 *scan_cnt, u8 *pending);

#endif

