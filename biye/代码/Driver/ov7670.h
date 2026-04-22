#ifndef __OV7670_H
#define __OV7670_H

#include "ov7670_fifo.h"

/* 单芯片本地 OV7670+FIFO：与 ov7670_fifo 同一套初始化 */
#define OV7670_Init()   OV7670_FIFO_Init()

#endif
