#ifndef __CAPTURE_H
#define __CAPTURE_H

#include "sys.h"
#include "capture_task.h"

extern volatile u8 g_capture_busy;
extern volatile u8 g_cap_last_ok;           /* 🟡12: 最近一次抓拍成功标志 */
extern volatile u16 g_cap_validation_fail_count; /* 🟡12: BMP校验失败计数 */
extern volatile u16 g_cap_q_drop;
extern volatile u16 g_cap_q_flush_ok;
extern volatile u16 g_cap_q_flush_fail;

void Bare_CapturePoll(void);
u8 Capture_LocalSnapshot(void);

#endif
