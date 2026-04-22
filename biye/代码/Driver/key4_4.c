#include "key4_4.h"
#include "delay.h"
#include "sys.h"
#include "board_config.h"
#include "stm32f10x_exti.h"
#include "misc.h"

#ifndef KEY_LONGPRESS_MS
#define KEY_LONGPRESS_MS  1000u
#endif
#ifndef KEY_STABLE_MATCHES
/* 连续 N 次 Key_Scan 得到相同 raw 才更新 s_stable（不依赖毫秒计时，适合矩阵抖动） */
#define KEY_STABLE_MATCHES  3u
#endif
/* 多击窗口（若日后恢复双击/三击逻辑时使用）；短按已改为松手立即上报，不再卡 300ms */
#ifndef KEY_MULTI_CLICK_MS
#define KEY_MULTI_CLICK_MS  300u
#endif

/* 扫描时序：可在 board_config.h 里用 KEY_MATRIX_* 覆盖 */
#ifndef KEY_MATRIX_DEBOUNCE_MS
#define KEY_MATRIX_DEBOUNCE_MS  15u
#endif
#ifndef KEY_MATRIX_SETTLE_US
#define KEY_MATRIX_SETTLE_US    400u
#endif
#ifndef KEY_MATRIX_READ_GAP_US
#define KEY_MATRIX_READ_GAP_US  100u
#endif

/*
 * 扫描方式：**列选通（Column strobe）**
 * - 行 X1~X4（接键盘 R1~R4）：上拉输入
 * - 列 Y1~Y4（接键盘 C1~C4）：推挽输出，空闲全高；轮流将某一列拉低，读行线谁为低即 (行,列)
 * 与旧版「行选通 + 读列」互为对偶，部分薄膜/长线在列驱动时更稳定。
 */

#define X1_GPIO_PORT BOARD_KEY_X1_PORT
#define X2_GPIO_PORT BOARD_KEY_X2_PORT
#define X3_GPIO_PORT BOARD_KEY_X3_PORT
#define X4_GPIO_PORT BOARD_KEY_X4_PORT

#define Y1_GPIO_PORT BOARD_KEY_Y1_PORT
#define Y2_GPIO_PORT BOARD_KEY_Y2_PORT
#define Y3_GPIO_PORT BOARD_KEY_Y3_PORT
#define Y4_GPIO_PORT BOARD_KEY_Y4_PORT

#define X1_GPIO_PIN BOARD_KEY_X1_PIN
#define X2_GPIO_PIN BOARD_KEY_X2_PIN
#define X3_GPIO_PIN BOARD_KEY_X3_PIN
#define X4_GPIO_PIN BOARD_KEY_X4_PIN

#define Y1_GPIO_PIN BOARD_KEY_Y1_PIN
#define Y2_GPIO_PIN BOARD_KEY_Y2_PIN
#define Y3_GPIO_PIN BOARD_KEY_Y3_PIN
#define Y4_GPIO_PIN BOARD_KEY_Y4_PIN

#define X1_RCC BOARD_KEY_X1_RCC
#define X2_RCC BOARD_KEY_X2_RCC
#define X3_RCC BOARD_KEY_X3_RCC
#define X4_RCC BOARD_KEY_X4_RCC

#define Y1_RCC BOARD_KEY_Y1_RCC
#define Y2_RCC BOARD_KEY_Y2_RCC
#define Y3_RCC BOARD_KEY_Y3_RCC
#define Y4_RCC BOARD_KEY_Y4_RCC

static void key_cols_all_high(void)
{
	GPIO_SetBits(Y1_GPIO_PORT, Y1_GPIO_PIN);
	GPIO_SetBits(Y2_GPIO_PORT, Y2_GPIO_PIN);
	GPIO_SetBits(Y3_GPIO_PORT, Y3_GPIO_PIN);
	GPIO_SetBits(Y4_GPIO_PORT, Y4_GPIO_PIN);
}

/* 空闲：列全低，行上拉；有按键时行经开关闭合到 GND，可 EXTI 唤醒（列全高时行无法反映按键） */
static void key_cols_all_low(void)
{
	GPIO_ResetBits(Y1_GPIO_PORT, Y1_GPIO_PIN);
	GPIO_ResetBits(Y2_GPIO_PORT, Y2_GPIO_PIN);
	GPIO_ResetBits(Y3_GPIO_PORT, Y3_GPIO_PIN);
	GPIO_ResetBits(Y4_GPIO_PORT, Y4_GPIO_PIN);
}

static void key_select_col_low(u8 col)
{
	key_cols_all_high();
	switch(col)
	{
	case 0: GPIO_ResetBits(Y1_GPIO_PORT, Y1_GPIO_PIN); break;
	case 1: GPIO_ResetBits(Y2_GPIO_PORT, Y2_GPIO_PIN); break;
	case 2: GPIO_ResetBits(Y3_GPIO_PORT, Y3_GPIO_PIN); break;
	case 3: GPIO_ResetBits(Y4_GPIO_PORT, Y4_GPIO_PIN); break;
	default: break;
	}
}

static u8 key_read_row_mask_low(void)
{
	u8 m = 0;
	if(GPIO_ReadInputDataBit(X1_GPIO_PORT, X1_GPIO_PIN) == Bit_RESET) m |= 0x01;
	if(GPIO_ReadInputDataBit(X2_GPIO_PORT, X2_GPIO_PIN) == Bit_RESET) m |= 0x02;
	if(GPIO_ReadInputDataBit(X3_GPIO_PORT, X3_GPIO_PIN) == Bit_RESET) m |= 0x04;
	if(GPIO_ReadInputDataBit(X4_GPIO_PORT, X4_GPIO_PIN) == Bit_RESET) m |= 0x08;
	return m;
}

static int key_index_from_mask(u8 mask)
{
	if(mask & 0x01) return 0;
	if(mask & 0x02) return 1;
	if(mask & 0x04) return 2;
	if(mask & 0x08) return 3;
	return -1;
}

/* 单调毫秒：SysTick 1ms（delay.c） */
static u32 key_monotonic_ms(void)
{
	return (u32)Bare_GetTickMs();
}

typedef enum {
	KEYSM_IDLE = 0,
	KEYSM_PRESSED,
	KEYSM_WAIT_REL
} keysm_t;

static keysm_t s_keysm = KEYSM_IDLE;
/* 上一个稳定键值：-1=无键；仅在三连相同 raw 后更新 */
static int s_stable = -1;
static int s_raw_q0 = -1;
static int s_raw_q1 = -1;
static int s_raw_q2 = -1;

static int s_active_key = -1;
static u32 s_press_t0 = 0;

static volatile KeyEvent s_last_event = KEY_EVENT_NONE;
static volatile int s_last_event_key = -1;

/* EXTI 唤醒：置位 pending，主循环轮询 Key_Scan */
volatile u8 g_key_exti_pending = 0;
static volatile u32 s_key_irq_count = 0;
static volatile u32 s_key_scan_count = 0;
static void Key_EXTI_NotifyFromISR(void);
static void Key_TaskNotify(void);

void Key_SetNotifyTask(void *task_handle)
{
	(void)task_handle;
}

static u8 key_shift_buffer_not_settled(void)
{
	return (s_raw_q0 != s_raw_q1 || s_raw_q1 != s_raw_q2) ? 1u : 0u;
}

int Key_NeedService(void)
{
	return (int)((g_key_exti_pending != 0u) || (s_keysm != KEYSM_IDLE)
			|| (key_shift_buffer_not_settled() != 0u));
}

/* 消抖中、长按等：需周期扫描 */
int Key_IsActiveScan(void)
{
	return (int)((s_keysm != KEYSM_IDLE) || (key_shift_buffer_not_settled() != 0u));
}

void Key_ForceService(void)
{
	g_key_exti_pending = 1u;
	/* 普通上下文下不能用 FromISR API */
	Key_TaskNotify();
}

void Key_Debug_GetCounters(u32 *irq_cnt, u32 *scan_cnt, u8 *pending)
{
	if(irq_cnt) *irq_cnt = (u32)s_key_irq_count;
	if(scan_cnt) *scan_cnt = (u32)s_key_scan_count;
	if(pending) *pending = (u8)g_key_exti_pending;
}

static void Key_EXTI_NotifyFromISR(void)
{
	(void)0;
}

static void Key_TaskNotify(void)
{
	(void)0;
}

static void Key_ROW_EXTI_Handler(uint32_t line)
{
	if(EXTI_GetITStatus(line) != RESET)
	{
		EXTI_ClearITPendingBit(line);
		s_key_irq_count++;
		g_key_exti_pending = 1u;
		Key_EXTI_NotifyFromISR();
	}
}

void EXTI0_IRQHandler(void)
{
	Key_ROW_EXTI_Handler(EXTI_Line0);
}

void EXTI1_IRQHandler(void)
{
	Key_ROW_EXTI_Handler(EXTI_Line1);
}

void EXTI2_IRQHandler(void)
{
	Key_ROW_EXTI_Handler(EXTI_Line2);
}

void EXTI3_IRQHandler(void)
{
	Key_ROW_EXTI_Handler(EXTI_Line3);
}

static void Key_EXTI_Init(void)
{
	u8 i;
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	static const uint32_t exti_lines[4] = { EXTI_Line0, EXTI_Line1, EXTI_Line2, EXTI_Line3 };
	static const uint8_t exti_irqs[4] = { EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn };

	for(i = 0; i < 4u; i++)
	{
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, i);
		EXTI_InitStructure.EXTI_Line = exti_lines[i];
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStructure);
	}

	for(i = 0; i < 4u; i++)
	{
		NVIC_InitStructure.NVIC_IRQChannel = exti_irqs[i];
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStructure);
	}
}

/* 三次按位多数表决，抗抖与长线 */
static u8 key_row_mask_majority(void)
{
	u8 a = key_read_row_mask_low();
	delay_us(KEY_MATRIX_READ_GAP_US);
	u8 b = key_read_row_mask_low();
	delay_us(KEY_MATRIX_READ_GAP_US);
	u8 c = key_read_row_mask_low();
	return (u8)((a & b) | (b & c) | (a & c));
}

void Key_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(X1_RCC|X2_RCC|X3_RCC|X4_RCC|Y1_RCC|Y2_RCC|Y3_RCC|Y4_RCC|RCC_APB2Periph_AFIO, ENABLE);

	/* 行：上拉输入（被选中列拉低时经按键读到低） */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = X1_GPIO_PIN; GPIO_Init(X1_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = X2_GPIO_PIN; GPIO_Init(X2_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = X3_GPIO_PIN; GPIO_Init(X3_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = X4_GPIO_PIN; GPIO_Init(X4_GPIO_PORT, &GPIO_InitStructure);

	/* 列：推挽输出；扫描时列选通，空闲见 key_cols_all_low */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = Y1_GPIO_PIN; GPIO_Init(Y1_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = Y2_GPIO_PIN; GPIO_Init(Y2_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = Y3_GPIO_PIN; GPIO_Init(Y3_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = Y4_GPIO_PIN; GPIO_Init(Y4_GPIO_PORT, &GPIO_InitStructure);

	Key_EXTI_Init();
	/* 空闲列全低，行线可被拉低，EXTI 才能感知按键 */
	key_cols_all_low();
}

/* 单次列选通扫描，得到当前稳定键码或 -1（无长按逻辑） */
static int key_scan_physical_once(void)
{
	static const u8 key_map[4][4] = {
		{1, 2, 3, 'A'},
		{4, 5, 6, 'B'},
		{7, 8, 9, 'C'},
		{'*', 0, '#', 'D'}
	};
	u8 col, row_mask;
	int row;

	key_cols_all_high();
	delay_us(200);

	for(col = 0; col < 4; col++)
	{
		key_select_col_low(col);
		delay_us(KEY_MATRIX_SETTLE_US);
		row_mask = key_row_mask_majority();
		if(!row_mask)
			continue;

		/* 须忙等：delay_ms 在 RTOS 下会 vTaskDelay，列选通期间切出可能导致误判/丢键 */
		delay_us(3000);
		key_select_col_low(col);
		delay_us(KEY_MATRIX_SETTLE_US);
		if(key_row_mask_majority() != row_mask)
			continue;

		row = key_index_from_mask(row_mask);
		if(row < 0)
			continue;

		delay_us(300);
		key_cols_all_low();
		return (int)key_map[row][col];
	}

	key_cols_all_low();
	return -1;
}

/* 三连相同 raw 才更新 s_stable（key_last 语义：仅稳定跃迁时变） */
static void key_stable_update(int raw)
{
	s_raw_q0 = s_raw_q1;
	s_raw_q1 = s_raw_q2;
	s_raw_q2 = raw;
#if (KEY_STABLE_MATCHES >= 3u)
	if(s_raw_q0 == s_raw_q1 && s_raw_q1 == s_raw_q2)
		s_stable = s_raw_q2;
#elif (KEY_STABLE_MATCHES == 2u)
	if(s_raw_q1 == s_raw_q2)
		s_stable = s_raw_q2;
#else
	s_stable = raw;
#endif
}

static void key_emit_event(KeyEvent ev, int code)
{
	s_last_event = ev;
	s_last_event_key = code;
}

KeyEvent Key_GetLastEvent(int *key_code)
{
	KeyEvent ev = (KeyEvent)s_last_event;
	if(key_code) *key_code = (int)s_last_event_key;
	s_last_event = KEY_EVENT_NONE;
	s_last_event_key = -1;
	return ev;
}

/*
 * 状态机只认 s_stable（消抖后的硬件稳定态）。
 * 当前策略：稳定按下即触发一次 CLICK，进入 WAIT_REL 等松手。
 * 这样按下就有反应，避免“必须按很久/松手才触发”的体感问题。
 */
int Key_Scan(void)
{
	int raw;
	u32 now;
	int ret = -1;

	s_key_scan_count++;
	g_key_exti_pending = 0u;
	raw = key_scan_physical_once();
	now = key_monotonic_ms();
	key_stable_update(raw);

	switch(s_keysm)
	{
	case KEYSM_IDLE:
		if(s_stable != -1)
		{
			s_active_key = s_stable;
			s_press_t0 = now;
			key_emit_event(KEY_EVENT_CLICK, s_active_key);
			ret = s_active_key;
			s_keysm = KEYSM_WAIT_REL;
		}
		break;

	case KEYSM_PRESSED:
		if(s_stable == s_active_key
				&& (u32)(now - s_press_t0) >= (u32)KEY_LONGPRESS_MS)
		{
			key_emit_event(KEY_EVENT_LONG_PRESS, s_active_key);
			ret = s_active_key;
			s_keysm = KEYSM_WAIT_REL;
		}
		else if(s_stable == -1)
		{
			if((u32)(now - s_press_t0) < (u32)KEY_LONGPRESS_MS)
			{
				key_emit_event(KEY_EVENT_CLICK, s_active_key);
				ret = s_active_key;
			}
			s_keysm = KEYSM_IDLE;
		}
		else if(s_stable != s_active_key)
		{
			s_keysm = KEYSM_IDLE;
		}
		break;

	case KEYSM_WAIT_REL:
		if(s_stable == -1)
		{
			s_keysm = KEYSM_IDLE;
		}
		else if(s_stable != s_active_key)
		{
			/* 用户快速从上一个键切到下一个键时，不要求必须经历完整的“稳定全松手”窗口，
			 * 否则会表现成前两次能输入、后续像卡住一样无响应。 */
			s_active_key = s_stable;
			s_press_t0 = now;
			key_emit_event(KEY_EVENT_CLICK, s_active_key);
			ret = s_active_key;
			s_keysm = KEYSM_WAIT_REL;
		}
		break;

	default:
		s_keysm = KEYSM_IDLE;
		break;
	}

	return ret;
}

u8 Key_Debug_ReadMask(void)
{
	u8 m = 0;
	key_cols_all_high();
	delay_us(50);
	if(GPIO_ReadInputDataBit(X1_GPIO_PORT, X1_GPIO_PIN)) m |= 0x01;
	if(GPIO_ReadInputDataBit(X2_GPIO_PORT, X2_GPIO_PIN)) m |= 0x02;
	if(GPIO_ReadInputDataBit(X3_GPIO_PORT, X3_GPIO_PIN)) m |= 0x04;
	if(GPIO_ReadInputDataBit(X4_GPIO_PORT, X4_GPIO_PIN)) m |= 0x08;
	if(GPIO_ReadInputDataBit(Y1_GPIO_PORT, Y1_GPIO_PIN)) m |= 0x10;
	if(GPIO_ReadInputDataBit(Y2_GPIO_PORT, Y2_GPIO_PIN)) m |= 0x20;
	if(GPIO_ReadInputDataBit(Y3_GPIO_PORT, Y3_GPIO_PIN)) m |= 0x40;
	if(GPIO_ReadInputDataBit(Y4_GPIO_PORT, Y4_GPIO_PIN)) m |= 0x80;
	key_cols_all_low();
	return m;
}
