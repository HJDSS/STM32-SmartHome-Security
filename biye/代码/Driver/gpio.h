#ifndef __GPIO_H
#define __GPIO_H
#include "sys.h"
#include "board_config.h"
#include "stm32f10x_gpio.h"

/* 门锁：与 board_config.h BOARD_DOOR_LOCK 一致（默认 PC6=PCout(6)）。PB12 仅作 SD_CS，勿作 RELAY */
#define RELAY  PCout(6)
#define GAS_VALVE_RELAY PCout(7)

/* ================== 蜂鸣器分级告警音类型 ================== */
typedef enum {
	BEEP_PATTERN_OFF = 0,
	BEEP_PATTERN_DOORBELL,      /* 门禁：三声短促 */
	BEEP_PATTERN_INTRUSION,     /* 入侵：急促连续 */
	BEEP_PATTERN_GAS,           /* 燃气：长鸣间隔 */
	BEEP_PATTERN_LOCKOUT,       /* 暴力破解锁定：交替鸣叫 */
} beep_pattern_t;

/* ================== 蜂鸣器 GPIO 直接操作（静态内联，供内部使用） ================== */
/* 蜂鸣器 PC10 开/关接口封装，极性由 board_config.h 的 BOARD_BEEP_ACTIVE_HIGH 控制 */
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

/* ================== 蜂鸣器节拍引擎（非阻塞） ================== */
/* 启动指定告警音模式（立即返回，由 BEEP_Tick10ms 驱动）；相同模式幂等 */
void BEEP_StartPattern(beep_pattern_t pattern);
/* 立即停止所有鸣叫 */
void BEEP_Stop(void);
/* 每 10ms 调用一次以推进节拍状态机 */
void BEEP_Tick10ms(void);

void BEEP_AND_RELAY_GPIO_Init(void);
void BEEP_Selftest_Extended(void);
void Actuator_EnterSafeState(void);

#endif
