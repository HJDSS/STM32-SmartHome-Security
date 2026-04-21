#ifndef __CAPTURE_H
#define __CAPTURE_H

#include "sys.h"
#include "capture_task.h"

extern volatile u8 g_capture_busy;

/* 主循环调用：OV7670+FIFO -> SD(BMP) */
void Bare_CapturePoll(void);

#endif
