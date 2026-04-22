#include <stm32f10x.h>
#include <stdio.h>
#include "board_config.h"
#include "usart1.h"
#if BOARD_PRINTF_SOFTUART_PC8
#include "soft_uart_tx.h"
#endif

char Usart1RecBuf[USART1_RXBUFF_SIZE];
unsigned int RxCounter = 0;

#if 1
#pragma import(__use_no_semihosting)
struct __FILE
{
	int handle;
};

FILE __stdout;

void _sys_exit(int x)
{
	x = x;
}

int fputc(int ch, FILE *f)
{
	(void)f;
#if BOARD_PRINTF_SOFTUART_PC8
	SoftUartTx_PC8_Putchar((u8)ch);
#else
	while ((USART1->SR & 0X40) == 0)
		;
	USART1->DR = (u8)ch;
#endif
	return ch;
}

#endif

void uart1_Init(u32 bound)
{
#if BOARD_PRINTF_SOFTUART_PC8
	SoftUartTx_PC8_Init();
#endif
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

	USART_DeInit(USART1);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_InitStructure.USART_BaudRate = bound;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(USART1, &USART_InitStructure);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART1, USART_IT_IDLE, DISABLE);
	USART_Cmd(USART1, ENABLE);
}

void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		if (RxCounter >= USART1_RXBUFF_SIZE)
			RxCounter = 0;
		Usart1RecBuf[RxCounter++] = USART_ReceiveData(USART1);
	}
	USART_ClearITPendingBit(USART1, USART_IT_RXNE);
}

void uart1_SendStr(char *SendBuf)
{
	while (*SendBuf)
	{
		while ((USART1->SR & 0X40) == 0)
			;
		USART1->DR = (u8)*SendBuf;
		SendBuf++;
	}
}

void uart1_send(unsigned char *bufs, unsigned char len)
{
	while (len--)
	{
		while ((USART1->SR & 0X40) == 0)
			;
		USART1->DR = (u8)*bufs;
		bufs++;
	}
}
