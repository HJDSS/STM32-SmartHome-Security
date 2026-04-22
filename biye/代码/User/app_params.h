#ifndef __APP_PARAMS_H
#define __APP_PARAMS_H

#include "board_config.h"

/* ================= 运行参数集中化（P1） =================
 * 说明：避免魔法数字散落在业务文件中，便于论文测试与调参复现。
 */

/* 传感器采样周期 */
#ifndef APP_DHT_POLL_MS
#define APP_DHT_POLL_MS            2000u
#endif
#ifndef APP_MQ2_POLL_MS
#define APP_MQ2_POLL_MS            250u
#endif

/* PIR 触发抑制窗口（非布防/高频触发时用于抑制） */
#ifndef APP_PIR_SUPPRESS_MS
#define APP_PIR_SUPPRESS_MS        3000u
#endif

/* 统计输出周期（串口打印，便于第6章测试统计） */
#ifndef APP_STAT_REPORT_MS
#define APP_STAT_REPORT_MS         30000u
#endif

/* 抓拍离线补传队列深度 */
#ifndef APP_CAP_OFFLINE_Q_DEPTH
#define APP_CAP_OFFLINE_Q_DEPTH    8u
#endif

#endif

