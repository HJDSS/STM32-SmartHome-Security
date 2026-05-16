#ifndef __CAPTURE_H
#define __CAPTURE_H

#include "sys.h"
#include "capture_task.h"

extern volatile u8 g_capture_busy;

void Bare_CapturePoll(void);
u8 Capture_LocalSnapshot(void);

#endif
