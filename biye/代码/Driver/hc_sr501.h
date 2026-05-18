#ifndef __HC_SR501_H
#define __HC_SR501_H

#include "board_config.h"

/* ------------------ 硬件引脚：统一在 User/board_config.h 的 BOARD_PIR_* ------------------
 * PIR 可放在任意支持 EXTI 的 GPIO：中断线与 IRQ 由引脚号决定（见下方映射）。
 * 触发与消抖逻辑见 hc_sr501.c。
 * --------------------------------------------------------------------------- */
#define HC_SR501_GPIO_PORT                 BOARD_PIR_GPIO_PORT
#define HC_SR501_GPIO_PIN                  BOARD_PIR_GPIO_PIN
#define HC_SR501_GPIO_CLK                  BOARD_PIR_GPIO_CLK
#define HC_SR501_GPIO_PORT_SOURCE          BOARD_PIR_GPIO_PORT_SOURCE
#define HC_SR501_GPIO_PIN_SOURCE           BOARD_PIR_GPIO_PIN_SOURCE

#define HC_SR501_EXTI_LINE                 BOARD_PIR_EXTI_LINE
#define HC_SR501_EXTI_IRQ                  BOARD_PIR_EXTI_IRQ
#define HC_SR501_EXTI_PREEMPT_PRIO         2
#define HC_SR501_EXTI_SUB_PRIO             2

/* 触发电平：1=高电平触发（上升沿进中断），0=低电平触发（下降沿进中断） */
#define HC_SR501_TRIGGER_LEVEL             1

/* 防抖延时（单位：ms） */
#define HC_SR501_DEBOUNCE_MS               80

extern volatile u8 g_hc_sr501_irq_flag;

void HC_SR501_Init(void);
void HC_SR501_EXTI_Init(void);
u8 HC_SR501_Read_Level(void);
u8 HC_SR501_Poll_Triggered(void);
u8 HC_SR501_IsValidTrigger(uint32_t now_ms);
u8 HC_SR501_IRQHandler_GetFlag(void);
void HC_SR501_IRQHandler_ClearFlag(void);

#endif
