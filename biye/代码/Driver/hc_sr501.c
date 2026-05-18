#include "hc_sr501.h"
#include "delay.h"
/* EXTI 结构体/宏定义来自 stm32f10x_exti.h（Keil C89/老版库需显式 include） */
#include "stm32f10x_exti.h"

#if USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "app_rtos.h"
#endif

volatile u8 g_hc_sr501_irq_flag = 0;

void HC_SR501_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(HC_SR501_GPIO_CLK | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Pin = HC_SR501_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HC_SR501_GPIO_PORT, &GPIO_InitStructure);
}

void HC_SR501_EXTI_Init(void)
{
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    GPIO_EXTILineConfig(HC_SR501_GPIO_PORT_SOURCE, HC_SR501_GPIO_PIN_SOURCE);

    EXTI_InitStructure.EXTI_Line = HC_SR501_EXTI_LINE;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    if(HC_SR501_TRIGGER_LEVEL)
    {
        EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    }
    else
    {
        EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    }
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = HC_SR501_EXTI_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = HC_SR501_EXTI_PREEMPT_PRIO;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = HC_SR501_EXTI_SUB_PRIO;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

u8 HC_SR501_Read_Level(void)
{
    return GPIO_ReadInputDataBit(HC_SR501_GPIO_PORT, HC_SR501_GPIO_PIN);
}

u8 HC_SR501_Poll_Triggered(void)
{
    static u8 debounce_cnt = 0;
    u8 level = HC_SR501_Read_Level();
    u8 debounce_tick = (HC_SR501_DEBOUNCE_MS + 9) / 10;

    if(level == HC_SR501_TRIGGER_LEVEL)
    {
        if(debounce_cnt < debounce_tick)
        {
            debounce_cnt++;
        }
        if(debounce_cnt >= debounce_tick)
        {
            debounce_cnt = debounce_tick;
            return 1;
        }
    }
    else
    {
        debounce_cnt = 0;
    }
    return 0;
}

u8 HC_SR501_IRQHandler_GetFlag(void)
{
    return g_hc_sr501_irq_flag;
}

void HC_SR501_IRQHandler_ClearFlag(void)
{
    g_hc_sr501_irq_flag = 0;
}

/* 中断入口：本工程 PIR 固定使用 PC4 -> EXTI4 */
void EXTI4_IRQHandler(void)
{
    if(EXTI_GetITStatus(HC_SR501_EXTI_LINE) != RESET)
    {
        g_hc_sr501_irq_flag = 1;
#if USE_FREERTOS
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            IPC_NotifyPIR_FromISR(&xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
#endif
        EXTI_ClearITPendingBit(HC_SR501_EXTI_LINE);
    }
}

