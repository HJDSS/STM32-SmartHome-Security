/**********************************************************************************
 * 文件名  ：usart2.c
 * 描述    ：ESP8266 走 **USART3（PB10/PB11）**，与 PA0~PA7 摄像头并口无冲突。
 *          函数名仍 UART2_* / USART2_Init_Config，兼容 esp8266_tls 与 Uart2_Buf。
 *          接收中断见 User/main.c 的 USART3_IRQHandler：同源写入 Uart2_Buf（OneNET/AT）与 Uart3_Buf（遥控解析）。
 * 库版本  ：ST_v3.5
 **********************************************************************************/

#include "usart2.h"
#include "delay.h"
#include "board_config.h"
#include "stm32f10x_gpio.h"
#include "misc.h"

#if NO_FREERTOS_MODE
/* 裸机：心跳 AT 等待期间须让出 CPU 扫矩阵键盘，否则长时间 delay 无键响应 */
extern void ESP8266_CooperativeYield(void);
#endif

void USART2_Init_Config(u32 bound)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef  NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(BOARD_USART3_APB1, ENABLE);
	RCC_APB2PeriphClockCmd(BOARD_USART3_GPIO_APB2 | RCC_APB2Periph_AFIO, ENABLE);

	GPIO_PinRemapConfig(GPIO_FullRemap_USART3, DISABLE);
	USART_DeInit(BOARD_USART3_INSTANCE);

	GPIO_InitStructure.GPIO_Pin = BOARD_USART3_TX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_USART3_TX_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = BOARD_USART3_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(BOARD_USART3_RX_PORT, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = bound;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(BOARD_USART3_INSTANCE, &USART_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(BOARD_USART3_INSTANCE, USART_IT_RXNE, ENABLE);

	USART_Cmd(BOARD_USART3_INSTANCE, ENABLE);
	USART_ClearFlag(BOARD_USART3_INSTANCE, USART_FLAG_TC);
}

void UART2_SendString(char* s)
{
	while(*s)
	{
		while(USART_GetFlagStatus(BOARD_USART3_INSTANCE, USART_FLAG_TC) == RESET)
			;
		USART_SendData(BOARD_USART3_INSTANCE, (u16)(*s++));
	}
}

void UART2_Send_Command(char* s)
{
	CLR_Buf2();
	UART2_SendString(s);
	UART2_SendString("\r");
}

static void UART2_WaitInterval_Cooperative(u32 interval_ms)
{
	u32 left = interval_ms;
	while(left > 0u)
	{
		u16 chunk = (left > 20u) ? 20u : (u16)left;
		delay_ms(chunk);
		left -= (u32)chunk;
#if NO_FREERTOS_MODE
		ESP8266_CooperativeYield();
#endif
	}
}

u8 UART2_Send_AT_Command(char *b, char *a, u8 wait_time, u32 interval_time)
{
	u8 i;
	i = 0;
	while(i < wait_time)
	{
		UART2_Send_Command(b);
		UART2_WaitInterval_Cooperative(interval_time);
		if(Find2(a))
		{
			return 1;
		}
		i++;
	}
	return 0;
}

void UART2_Send_Command_END(char* s)
{
	CLR_Buf2();
	UART2_SendString(s);
}

u8 UART2_Send_AT_Command_End(char *b, char *a, u8 wait_time, u32 interval_time)
{
	u8 i;
	i = 0;
	while(i < wait_time)
	{
		UART2_Send_Command_END(b);
		UART2_WaitInterval_Cooperative(interval_time);
		if(Find2(a))
		{
			return 1;
		}
		i++;
	}
	return 0;
}
