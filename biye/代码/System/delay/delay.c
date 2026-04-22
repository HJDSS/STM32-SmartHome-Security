#include "delay.h"
#include "sys.h"
#include "bare_task_sched.h"

static u8 fac_us = 0;

volatile u32 g_bare_tick_ms = 0;

u32 Bare_GetTickMs(void)
{
	return g_bare_tick_ms;
}

void delay_init(void)
{
	fac_us = (u8)(SystemCoreClock / 8000000);
	if(fac_us == 0)
		fac_us = 9;
	if(SysTick_Config(SystemCoreClock / 1000UL))
	{
		while(1) {}
	}
	g_bare_tick_ms = 0;
	BareTaskSched_Init();
}

void delay_us(u32 nus)
{
	volatile u32 cnt;
	while(nus--)
	{
		cnt = fac_us * 4;
		while(cnt--)
			__NOP();
	}
}

void delay_ms(u16 nms)
{
	u16 n = nms;
	if(n == 0)
		return;
	while(n--)
		delay_us(1000);
}
