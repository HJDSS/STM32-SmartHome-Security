#ifndef __BARE_TASK_SCHED_H
#define __BARE_TASK_SCHED_H

#include "sys.h"

/* 裸机协作式周期调度：主循环 BareTaskSched_Run 内用系统毫秒节拍判断是否到期。
 * 槽位仅从低索引紧填（Add 不删槽），Run 只扫描 s_task_count 个槽，为 O(n)。
 * 任务内勿长时间阻塞；长逻辑请拆周期或状态机。 */

#ifndef BARE_TASK_MAX
#define BARE_TASK_MAX  8u
#endif

typedef struct
{
	void (*task)(void);
	u32 period_ms;
	u32 last_ms;
	u8 used;
} bare_task_t;

void BareTaskSched_Init(void);
s8 BareTaskSched_Add(void (*task_fn)(void), u32 period_ms);
void BareTaskSched_Run(void);

#endif
