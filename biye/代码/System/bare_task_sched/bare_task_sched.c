#include "bare_task_sched.h"
#include "delay.h"

static bare_task_t s_tasks[BARE_TASK_MAX];
static u8 s_task_count;

void BareTaskSched_Init(void)
{
	u8 i;
	s_task_count = 0;
	for(i = 0; i < BARE_TASK_MAX; i++)
	{
		s_tasks[i].used = 0;
		s_tasks[i].task = 0;
		s_tasks[i].period_ms = 0;
		s_tasks[i].last_ms = 0;
	}
}

s8 BareTaskSched_Add(void (*task_fn)(void), u32 period_ms)
{
	u8 i;
	u32 t;
	if(task_fn == 0 || period_ms == 0)
		return -1;
	t = Bare_GetTickMs();
	for(i = 0; i < BARE_TASK_MAX; i++)
	{
		if(!s_tasks[i].used)
		{
			s_tasks[i].used = 1;
			s_tasks[i].task = task_fn;
			s_tasks[i].period_ms = period_ms;
			s_tasks[i].last_ms = t;
			s_task_count++;
			return (s8)i;
		}
	}
	return -1;
}

void BareTaskSched_Run(void)
{
	u8 i, n;
	u32 now;
	void (*fn)(void);

	n = s_task_count;
	if(n == 0)
		return;
	now = Bare_GetTickMs();
	/* 槽位自 0 起连续占用，无空洞 */
	for(i = 0; i < n; i++)
	{
		fn = s_tasks[i].task;
		if(fn == 0)
			continue;
		if((u32)(now - s_tasks[i].last_ms) >= s_tasks[i].period_ms)
		{
			s_tasks[i].last_ms = now;
			fn();
		}
	}
}
