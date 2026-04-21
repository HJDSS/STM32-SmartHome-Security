#ifndef __GPIO_H
#define __GPIO_H
#include "sys.h"
#include "board_config.h"
#include "stm32f10x_gpio.h"

/* 门锁：与 board_config.h BOARD_DOOR_LOCK 一致（默认 PC6=PCout(6)）。PB12 仅作 SD_CS，勿作 RELAY */
#define RELAY  PCout(6)

/* ������ PC10����/ͣ�ɽӿڷ�װ�����Լ� board_config.h �� BOARD_BEEP_ACTIVE_HIGH */
static inline void BEEP_SoundOn(void)
{
#if BOARD_BEEP_ACTIVE_HIGH
	GPIO_SetBits(BOARD_BEEP_PORT, BOARD_BEEP_PIN);
#else
	GPIO_ResetBits(BOARD_BEEP_PORT, BOARD_BEEP_PIN);
#endif
}

static inline void BEEP_SoundOff(void)
{
#if BOARD_BEEP_ACTIVE_HIGH
	GPIO_ResetBits(BOARD_BEEP_PORT, BOARD_BEEP_PIN);
#else
	GPIO_SetBits(BOARD_BEEP_PORT, BOARD_BEEP_PIN);
#endif
}

static inline void BEEP_Toggle(void)
{
	if (GPIO_ReadOutputDataBit(BOARD_BEEP_PORT, BOARD_BEEP_PIN))
		GPIO_ResetBits(BOARD_BEEP_PORT, BOARD_BEEP_PIN);
	else
		GPIO_SetBits(BOARD_BEEP_PORT, BOARD_BEEP_PIN);
}

void BEEP_AND_RELAY_GPIO_Init(void);
void BEEP_Selftest_Extended(void);

#endif
