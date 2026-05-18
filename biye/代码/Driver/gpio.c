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

/* ================== 蜂鸣器节拍状态机 ================== */
typedef struct {
	beep_pattern_t pattern;     /* 当前模式 */
	u8            phase;        /* 0=停, 1=响 */
	u8            tick;         /* 当前节拍内累计 10ms 拍数 */
	u8            repeat;       /* 剩余重复次数（0=无限） */
} beep_state_t;

static beep_state_t g_beep;

void BEEP_StartPattern(beep_pattern_t pattern)
{
	if (pattern == BEEP_PATTERN_OFF) {
		BEEP_Stop();
		return;
	}
	/* 相同模式幂等：避免重复调用重置节拍 */
	if (g_beep.pattern == pattern)
		return;

	g_beep.pattern = pattern;
	g_beep.phase   = 1u;   /* 从响阶段开始 */
	g_beep.tick    = 0u;

	switch (pattern) {
	case BEEP_PATTERN_DOORBELL:
		g_beep.repeat = (u8)BOARD_BEEP_DOORBELL_REPEAT;
		break;
	case BEEP_PATTERN_INTRUSION:
	case BEEP_PATTERN_GAS:
	case BEEP_PATTERN_LOCKOUT:
	default:
		g_beep.repeat = 0u;  /* 无限循环，由 BEEP_Stop 终止 */
		break;
	}

	BEEP_SoundOn();
}

void BEEP_Stop(void)
{
	g_beep.pattern = BEEP_PATTERN_OFF;
	g_beep.phase   = 0u;
	g_beep.tick    = 0u;
	g_beep.repeat  = 0u;
	BEEP_SoundOff();
}

void BEEP_Tick10ms(void)
{
	u8 on_ticks, off_ticks;

	if (g_beep.pattern == BEEP_PATTERN_OFF)
		return;

	switch (g_beep.pattern) {
	case BEEP_PATTERN_DOORBELL:
		on_ticks  = (u8)(BOARD_BEEP_DOORBELL_ON_MS  / 10u);
		off_ticks = (u8)(BOARD_BEEP_DOORBELL_OFF_MS / 10u);
		break;
	case BEEP_PATTERN_INTRUSION:
		on_ticks  = (u8)(BOARD_BEEP_INTRUSION_ON_MS  / 10u);
		off_ticks = (u8)(BOARD_BEEP_INTRUSION_OFF_MS / 10u);
		break;
	case BEEP_PATTERN_GAS:
		on_ticks  = (u8)(BOARD_BEEP_GAS_ON_MS  / 10u);
		off_ticks = (u8)(BOARD_BEEP_GAS_OFF_MS / 10u);
		break;
	case BEEP_PATTERN_LOCKOUT:
		on_ticks  = (u8)(BOARD_BEEP_LOCKOUT_ON_MS  / 10u);
		off_ticks = (u8)(BOARD_BEEP_LOCKOUT_OFF_MS / 10u);
		break;
	default:
		return;
	}

	if (++g_beep.tick >= (g_beep.phase ? on_ticks : off_ticks)) {
		g_beep.tick  = 0u;
		g_beep.phase ^= 1u;   /* 切换响/停阶段 */

		if (g_beep.phase) {
			BEEP_SoundOn();
		} else {
			BEEP_SoundOff();
			/* 有限次数模式：每次进入停阶段消耗一次重复 */
			if (g_beep.pattern == BEEP_PATTERN_DOORBELL
			    && g_beep.repeat > 0u) {
				if (--g_beep.repeat == 0u)
					BEEP_Stop();
			}
		}
	}
}
