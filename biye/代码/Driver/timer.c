#include "timer.h"
#include "delay.h"
#include "sys.h"
#include "gpio.h"

/* TIM2：1s tick（100 * 10ms），门锁倒计时与 pass 超时 */
void TIM2_Init(u16 arr, u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	TIM_TimeBaseStructure.TIM_Period = arr;
	TIM_TimeBaseStructure.TIM_Prescaler = psc;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM2, ENABLE);
}

extern u8 RELAY_TIME;
extern u8 InitDisplay;
extern u8 pass;
extern u8 ReInputEn;

void TIM2_IRQHandler(void)
{
	static u8 time_count = 0;

	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		if (time_count++ >= 100)
		{
			time_count = 0;

			/* 🟡4: 定时自动锁止 — 非论文描述的"二次驱动"反馈式闭环
			 * 原因：硬件无门磁/限位开关/电流检测引脚，无法检测锁具状态
			 * 当前为单次定时锁止：RELAY_TIME递减至0后RELAY=1(失电锁止)
			 * 若需真正的二次驱动，需接入门磁传感器并置 APP_ACTUATOR_FEEDBACK_ENABLE=1
			 */
			if (RELAY_TIME)
			{
				RELAY_TIME--;
				if (RELAY_TIME == 0)
					RELAY = 1;
			}
			else
			{
				if (pass == 1)
				{
					if (ReInputEn == 0)
					{
						pass = 0;
						InitDisplay = 1;
					}
				}
			}
		}
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
