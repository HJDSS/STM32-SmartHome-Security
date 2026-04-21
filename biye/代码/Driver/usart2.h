#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"

#define USART1_REC_MAXLEN 200	//���������ݳ���

/* ���ջ������建��/������ User/main.c ��ʵ�֣�USART2_IRQHandler д�� */
/* Uart2_Buf 由 USART3_IRQHandler 写入（ESP 使用 USART3 PB10/11） */
extern char Uart2_Buf[];
void CLR_Buf2(void);
u8 Find2(char *a);

void USART2_Init_Config(u32 bound);
void UART2_SendString(char* s);

void UART2_SendLR(void);
void UART2_Send_Command(char* s);
u8 UART2_Send_AT_Command(char *b,char *a,u8 wait_time,u32 interval_time);
void UART2_Send_Command_END(char* s); 
u8 UART2_Send_AT_Command_End(char *b,char *a,u8 wait_time,u32 interval_time);


//����2���ͻس�����
#define UART2_SendLR() UART2_SendString("\r\n")
//											
#endif


