#ifndef __SOFT_UART_TX_H
#define __SOFT_UART_TX_H

#include "board_config.h"

#if BOARD_PRINTF_SOFTUART_PC8

#include "sys.h"

void SoftUartTx_PC8_Init(void);
void SoftUartTx_PC8_Putchar(u8 c);

#endif

#endif
