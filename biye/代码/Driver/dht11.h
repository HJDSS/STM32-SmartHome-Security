#ifndef __DHT11_H
#define __DHT11_H

#include "board_config.h"

/*
 * DHT11 单总线：引脚在 User/board_config.h（BOARD_DHT11_*，当前工程保持原引脚不变）。
 * 时序按照参考程序的单总线思路实现，但仍复用工程现有 GPIO 定义。
 */

#define DHT11_GPIO_PORT              BOARD_DHT11_GPIO_PORT
#define DHT11_GPIO_PIN               BOARD_DHT11_GPIO_PIN
#define DHT11_GPIO_CLK               BOARD_DHT11_GPIO_CLK
#define DHT_SENSOR_TYPE              BOARD_DHT_SENSOR_TYPE

#define DHT11_TIMEOUT_US             BOARD_DHT_WAIT_TIMEOUT_US

/* DHT11 需要上拉保持空闲高电平：输入上拉 + 开漏输出更稳 */
#define DHT11_IO_IN()                DHT11_Set_IO_Mode(GPIO_Mode_IPU)
#define DHT11_IO_OUT()               DHT11_Set_IO_Mode(GPIO_Mode_Out_OD)

#define DHT11_DQ_OUT(v)              GPIO_WriteBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN, (BitAction)(v))
#define DHT11_DQ_IN                  GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN)

u8 DHT11_Init(void);
u8 DHT11_Read_Data(u8 *temp, u8 *humi);
u8 DHT11_Read_Byte(void);
u8 DHT11_Read_Bit(void);
u8 DHT11_Check(void);
void DHT11_Rst(void);
void DHT11_Set_IO_Mode(GPIOMode_TypeDef mode);
u8 DHT11_GetLastError(void); /* 0=OK 1=CHK 2=TIMEOUT 3=CRC */
u8 DHT11_GetLastDQ(void);
u8 DHT11_GetLastResp(void);
void DHT11_Debug_ReadFrame(char *out);

#endif
