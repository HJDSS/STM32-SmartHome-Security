#ifndef __OV7670CFG_QVGA_RGB565_H
#define __OV7670CFG_QVGA_RGB565_H

#include "sys.h"

extern const u8 ov7670_init_reg_tbl[][2];
/* 与 ov7670cfg_qvga_rgb565.c 中表项数一致，避免对不完整类型 sizeof（Keil 告警） */
#define OV7670_INIT_REG_TBL_LEN    (20u)

#endif

