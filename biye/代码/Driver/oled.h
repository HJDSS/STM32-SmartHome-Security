#ifndef __OLED_H
#define __OLED_H 

#include "sys.h"
#include "stdlib.h"
#include "board_config.h"

//----------------- OLED 端口（引脚见 board_config.h）----------------

/* 0.96" I2C：SSD1306 / SSD1315 等 128x64 或 128x32
 * - I2C 7-bit 0x3C(写 0x78) 或 0x3D(写 0x7A)，OLED_Init 内自动探测
 */
#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR  0x78
#endif
#ifndef OLED_HEIGHT
#define OLED_HEIGHT    64
#endif

#define OLED_SCLK_Clr() GPIO_ResetBits(BOARD_OLED_GPIO_PORT, BOARD_OLED_PIN_SCL)
#define OLED_SCLK_Set() GPIO_SetBits(BOARD_OLED_GPIO_PORT, BOARD_OLED_PIN_SCL)

#define OLED_SDIN_Clr() GPIO_ResetBits(BOARD_OLED_GPIO_PORT, BOARD_OLED_PIN_SDA)
#define OLED_SDIN_Set() GPIO_SetBits(BOARD_OLED_GPIO_PORT, BOARD_OLED_PIN_SDA)


#define OLED_CMD  0	//д����
#define OLED_DATA 1	//д����
#define u8 unsigned char
#define u32 unsigned int

void OLED_ShowChinese32(u8 x,u8 y,u8 num,u8 size1);      //��ʾ����  32x32
void OLED_ShowChinese16(u8 x,u8 y,u8 num,u8 size1);      //��ʾ����  16x16
void OLED_ShowChinese(u8 x,u8 y,u8 num,u8 size1);
void OLED_ClearPoint(u8 x,u8 y);
void OLED_ColorTurn(u8 i);
void OLED_DisplayTurn(u8 i);
void I2C_Start(void);
void I2C_Stop(void);
u8 I2C_WaitAck(void);
void Send_Byte(u8 dat);
void OLED_WR_Byte(u8 dat,u8 mode);
void OLED_DisPlay_On(void);
void OLED_DisPlay_Off(void);
void OLED_Refresh(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x,u8 y);
void OLED_ShowChar(u8 x,u8 y,char chr,u8 size1);
void OLED_ShowCharu8(u8 x,u8 y,u8 chr,u8 size1);
void OLED_ShowString(u8 x,u8 y,char chr[],u8 size1);
void OLED_ShowStringu8(u8 x,u8 y,u8 *chr,u8 size1);
void OLED_WR_BP(u8 x,u8 y);
void OLED_Init(void);
/* 连续多次 ShowString/汉字时包在 Begin/End 之间，只全屏刷新一次，减轻 I2C 负载 */
void OLED_BatchBegin(void);
void OLED_BatchEnd(void);

/* 上电快速自检：首列竖线 + 边界字符；用于判定 SSD1306/SH1106 列偏移匹配度 */
void OLED_SelfTest_PanelAndColumn(void);

/* 上电自检：仅探测 I2C 地址是否有 ACK（1:有 0:无） */
u8 OLED_Probe(void);
/* OLED_Init 自动选地址后，当前使用的 8 位写地址（0x78 或 0x7A） */
u8 OLED_GetActiveI2cAddr(void);
/* OLED_Init 阶段 I2C 是否探测到从机 ACK（勿在已显示内容后再次 OLED_Probe） */
u8 OLED_GetProbeOk(void);

void huanying(void);




#endif



