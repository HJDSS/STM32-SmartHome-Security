#ifndef __OV7670_FIFO_H
#define __OV7670_FIFO_H

#include "sys.h"
#include "board_config.h"

#define OV_VSYNC_PORT        BOARD_OV_VSYNC_PORT
#define OV_VSYNC_PIN         BOARD_OV_VSYNC_PIN
#define OV_VSYNC_READ()      GPIO_ReadInputDataBit(OV_VSYNC_PORT,OV_VSYNC_PIN)

#define OV_WRST_PORT         BOARD_OV_WRST_PORT
#define OV_WRST_PIN          BOARD_OV_WRST_PIN
#define OV_WRST(v)           GPIO_WriteBit(OV_WRST_PORT,OV_WRST_PIN,(BitAction)(v))

#define OV_RRST_PORT         BOARD_OV_RRST_PORT
#define OV_RRST_PIN          BOARD_OV_RRST_PIN
#define OV_RRST(v)           GPIO_WriteBit(OV_RRST_PORT,OV_RRST_PIN,(BitAction)(v))

#define OV_WREN_PORT         BOARD_OV_WREN_PORT
#define OV_WREN_PIN          BOARD_OV_WREN_PIN
#define OV_WREN(v)           GPIO_WriteBit(OV_WREN_PORT,OV_WREN_PIN,(BitAction)(v))

#define OV_OE_PORT           BOARD_OV_OE_PORT
#define OV_OE_PIN            BOARD_OV_OE_PIN
#define OV_OE(v)             GPIO_WriteBit(OV_OE_PORT,OV_OE_PIN,(BitAction)(v))

#define OV_RCK_PORT          BOARD_OV_RCK_PORT
#define OV_RCK_PIN           BOARD_OV_RCK_PIN
#define OV_RCK_H()           GPIO_SetBits(OV_RCK_PORT,OV_RCK_PIN)
#define OV_RCK_L()           GPIO_ResetBits(OV_RCK_PORT,OV_RCK_PIN)

#define OV_DATA_PORT         BOARD_OV_DATA_PORT
#define OV_DATA_READ()       ((u8)(OV_DATA_PORT->IDR & 0x00FF))

u8 OV7670_FIFO_Init(void);
void OV7670_FIFO_StartCapture(void);
u8 OV7670_FIFO_StartCaptureTimeout(u32 timeout_ms);
void OV7670_FIFO_ResetReadPtr(void);
u8 OV7670_FIFO_ReadByte(void);
void OV7670_DebugGetId(u8 *pid, u8 *ver);

#endif

