#ifndef __SCCB_H
#define __SCCB_H

#include "sys.h"
#include "board_config.h"

// SCCB引脚(统一在board_config.h配置)
#define SCCB_SCL_PORT      BOARD_SCCB_SCL_PORT
#define SCCB_SCL_PIN       BOARD_SCCB_SCL_PIN
#define SCCB_SDA_PORT      BOARD_SCCB_SDA_PORT
#define SCCB_SDA_PIN       BOARD_SCCB_SDA_PIN
#define SCCB_GPIO_CLK      BOARD_SCCB_GPIO_CLK

#define SCCB_SCL(v)        GPIO_WriteBit(SCCB_SCL_PORT,SCCB_SCL_PIN,(BitAction)(v))
#define SCCB_SDA(v)        GPIO_WriteBit(SCCB_SDA_PORT,SCCB_SDA_PIN,(BitAction)(v))
#define SCCB_READ_SDA      GPIO_ReadInputDataBit(SCCB_SDA_PORT,SCCB_SDA_PIN)

#define SCCB_ID            0X42

void SCCB_Init(void);
u8 SCCB_WR_Reg(u8 reg,u8 data);
u8 SCCB_RD_Reg(u8 reg);

#endif

