#include "board_config.h"

#if BOARD_PRINTF_SOFTUART_PC8

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "delay.h"
#include "soft_uart_tx.h"

/* PC8 与照明继电器同脚；继电器未接时作调试 TX。波特率见 board_config.h BOARD_SOFTUART_BAUD */
#define SOFTUART_TX_PORT   BOARD_LIGHT_RELAY_PORT
#define SOFTUART_TX_PIN    BOARD_LIGHT_RELAY_PIN
#define SOFTUART_TX_CLK    BOARD_LIGHT_RELAY_CLK

#ifndef BOARD_SOFTUART_BAUD
#define BOARD_SOFTUART_BAUD  57600u
#endif

#define SOFTUART_BIT_US  ((u16)(1000000u / BOARD_SOFTUART_BAUD))

void SoftUartTx_PC8_Init(void)
{
	GPIO_InitTypeDef g;

	RCC_APB2PeriphClockCmd(SOFTUART_TX_CLK, ENABLE);
	g.GPIO_Pin = SOFTUART_TX_PIN;
	g.GPIO_Mode = GPIO_Mode_Out_PP;
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SOFTUART_TX_PORT, &g);
	GPIO_SetBits(SOFTUART_TX_PORT, SOFTUART_TX_PIN);
}

void SoftUartTx_PC8_Putchar(u8 c)
{
	u8 i;
	u16 bit_us = SOFTUART_BIT_US;
	if (bit_us < 2u)
		bit_us = 2u;

	GPIO_ResetBits(SOFTUART_TX_PORT, SOFTUART_TX_PIN);
	delay_us(bit_us);
	for (i = 0; i < 8u; i++) {
		if (c & 0x01u)
			GPIO_SetBits(SOFTUART_TX_PORT, SOFTUART_TX_PIN);
		else
			GPIO_ResetBits(SOFTUART_TX_PORT, SOFTUART_TX_PIN);
		c >>= 1;
		delay_us(bit_us);
	}
	GPIO_SetBits(SOFTUART_TX_PORT, SOFTUART_TX_PIN);
	delay_us(bit_us);
}

#endif /* BOARD_PRINTF_SOFTUART_PC8 */
