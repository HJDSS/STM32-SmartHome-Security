#include "gpio.h"
#include "board_config.h"
#include "delay.h"

//////////////////////////////////////////////////////////////////////////////////
// 继电器与蜂鸣器 GPIO
//////////////////////////////////////////////////////////////////////////////////

void BEEP_AND_RELAY_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(BOARD_DOOR_LOCK_CLK | BOARD_GAS_VALVE_RELAY_CLK | BOARD_BEEP_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Pin = BOARD_DOOR_LOCK_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_DOOR_LOCK_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(BOARD_DOOR_LOCK_PORT, BOARD_DOOR_LOCK_PIN);

	GPIO_InitStructure.GPIO_Pin = BOARD_BEEP_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_BEEP_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = BOARD_GAS_VALVE_RELAY_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_GAS_VALVE_RELAY_PORT, &GPIO_InitStructure);

	Actuator_EnterSafeState();
}

void Actuator_EnterSafeState(void)
{
	/* 默认安全态：门锁恢复默认关闭、燃气阀关闭、蜂鸣器关闭 */
	RELAY = 1;
	GAS_VALVE_RELAY = 1;
	BEEP_SoundOff();
}

void BEEP_Selftest_Extended(void)
{
	unsigned i;

	BEEP_SoundOff();
	delay_ms(80);
	/* 有源：三次中长鸣（BOARD_BEEP_ACTIVE_HIGH 由 BEEP_SoundOn/Off 适配） */
	for (i = 0; i < 3u; i++) {
		BEEP_SoundOn();
		delay_ms(200);
		BEEP_SoundOff();
		delay_ms(120);
	}
	delay_ms(200);
	/* 无源：方波约 2kHz、0.4s（有源模块此段可能仅轻微咔声，可忽略） */
	for (i = 0; i < 800u; i++) {
		BEEP_SoundOn();
		delay_us(250);
		BEEP_SoundOff();
		delay_us(250);
	}
	BEEP_SoundOff();
}
