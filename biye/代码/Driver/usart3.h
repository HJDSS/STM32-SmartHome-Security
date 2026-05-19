#ifndef __USART3_H
#define __USART3_H

#include "stm32f10x.h"

#define USART3_REC_MAXLEN 200	//最大接收数据长度



void USART3_Init_Config(u32 bound);
void UART3_SendString(char* s);
void UART3_SendLR(void);
void UART3_Send_Command(char* s);
u8 UART3_Send_AT_Command(char *b,char *a,u8 wait_time,u32 interval_time);
void UART3_Send_Command_END(char* s); 
u8 UART3_Send_AT_Command_End(char *b,char *a,u8 wait_time,u32 interval_time);


//串口2发送回车换行
#define UART3_SendLR() UART3_SendString("\r\n")
//											
#endif


