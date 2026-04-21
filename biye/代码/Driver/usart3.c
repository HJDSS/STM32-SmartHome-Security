/**********************************************************************************
 * 文件：usart3.c
 * 说明：USART3 初始化与收发（引脚以 board_config.h 为准）
 * 版本：ST_v3.5
 **********************************************************************************/

#include "usart3.h"
#include "delay.h"
#include "board_config.h"

extern void CLR_Buf(void);
extern u8 Find(char *a);

/*******************************************************************************
 * ??????  : USART3_Init_Config
 * ????    : USART3 ???????8N1?????????? board_config.h ??????
 * ????    : bound ??????????????????? 115200??
 *******************************************************************************/
void USART3_Init_Config(u32 bound)
{
	/* ESP8266 已占用 USART3（PB10/11），由 USART2_Init_Config() 完成初始化；此处保留空实现以免旧调用 HardFault */
	(void)bound;
}

/*******************************************************************************
 * ??????  : UART3_SendString
 * ????    : ????????? '\0' ??��???????
 *******************************************************************************/
void UART3_SendString(char* s)
{
	while(*s)
	{
		while(USART_GetFlagStatus(BOARD_USART3_INSTANCE, USART_FLAG_TC) == RESET)
			;
		USART_SendData(BOARD_USART3_INSTANCE, (u16)(*s++));
	}
}

void UART3_Send_Command(char* s)
{
	CLR_Buf();
	UART3_SendString(s);
	UART3_SendString("\r\n");
}

/*******************************************************************************
 * ?????? : UART3_Send_AT_Command
 * ????   : ?? AT ?????????? interval_time ms??
 *******************************************************************************/
extern u8 Find(char *a);

u8 UART3_Send_AT_Command(char *b, char *a, u8 wait_time, u32 interval_time)
{
	u8 i;
	i = 0;
	while(i < wait_time)
	{
		UART3_Send_Command(b);
		delay_ms(interval_time);
		if(Find(a))
		{
			return 1;
		}
		i++;
	}

	return 0;
}

void UART3_Send_Command_END(char* s)
{
	CLR_Buf();
	UART3_SendString(s);
}

u8 UART3_Send_AT_Command_End(char *b, char *a, u8 wait_time, u32 interval_time)
{
	u8 i;
	i = 0;
	while(i < wait_time)
	{
		UART3_Send_Command_END(b);
		delay_ms(interval_time);
		if(Find(a))
		{
			return 1;
		}
		i++;
	}

	return 0;
}
