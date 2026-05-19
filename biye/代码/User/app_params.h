#ifndef __APP_PARAMS_H
#define __APP_PARAMS_H

#include "board_config.h"

#ifndef APP_DHT_POLL_MS
#define APP_DHT_POLL_MS            2000u
#endif
#ifndef APP_DHT_RETRY_BACKOFF_MAX_MS
#define APP_DHT_RETRY_BACKOFF_MAX_MS 16000u
#endif
#ifndef APP_DHT_OFFLINE_FAILS
#define APP_DHT_OFFLINE_FAILS      3u
#endif
#ifndef APP_DHT_RECOVER_SUCCESSES
#define APP_DHT_RECOVER_SUCCESSES  2u
#endif
#ifndef APP_MQ2_POLL_MS
#define APP_MQ2_POLL_MS            250u
#endif
#ifndef APP_MQ2_STUCK_DIFF_ADC
#define APP_MQ2_STUCK_DIFF_ADC     3u
#endif
#ifndef APP_MQ2_STUCK_COUNT
#define APP_MQ2_STUCK_COUNT        40u
#endif
#ifndef APP_MQ2_RECOVER_GOOD_COUNT
#define APP_MQ2_RECOVER_GOOD_COUNT 4u
#endif

#ifndef APP_PIR_SUPPRESS_MS
#define APP_PIR_SUPPRESS_MS        3000u
#endif
#ifndef APP_PIR_CLEAR_LOW_MS
#define APP_PIR_CLEAR_LOW_MS       350u
#endif
#ifndef APP_PIR_MIN_ACTIVE_MS
#define APP_PIR_MIN_ACTIVE_MS      200u
#endif
#ifndef APP_PIR_DEBOUNCE_DEFAULT
#define APP_PIR_DEBOUNCE_DEFAULT   80u    /* 🟡13: PIR消抖默认值(ms) */
#endif
#ifndef APP_PIR_DEBOUNCE_MIN
#define APP_PIR_DEBOUNCE_MIN       20u    /* 🟡13: PIR消抖最小可调值(ms) */
#endif
#ifndef APP_PIR_DEBOUNCE_MAX
#define APP_PIR_DEBOUNCE_MAX       200u   /* 🟡13: PIR消抖最大可调值(ms) */
#endif

#ifndef APP_LOCK_MAX_FAIL
#define APP_LOCK_MAX_FAIL          3u
#endif
#ifndef APP_LOCK_BRUTE_MAX
#define APP_LOCK_BRUTE_MAX         5u
#endif
#ifndef APP_LOCK_LOCKOUT_MS
#define APP_LOCK_LOCKOUT_MS        30000u
#endif
#ifndef APP_BRUTE_BEEP_MS
#define APP_BRUTE_BEEP_MS          3000u
#endif
#ifndef APP_ADMIN_SESSION_MS
#define APP_ADMIN_SESSION_MS       20000u  /* 管理员窗口默认 20s */
#endif
#ifndef APP_ADMIN_SESSION_MAX_MS
#define APP_ADMIN_SESSION_MAX_MS   60000u  /* 录入/改密期间延长至 60s */
#endif
#ifndef APP_LOCK_OPEN_HOLD_MS
#define APP_LOCK_OPEN_HOLD_MS      3000u   /* 开锁保持时间(ms) — 论文要求3秒 */
#endif
#ifndef APP_LOCK_OPEN_HOLD_TICKS
#define APP_LOCK_OPEN_HOLD_TICKS   ((u8)(APP_LOCK_OPEN_HOLD_MS / 1000u))  /* TIM2每1秒递减1，15→3秒 */
#endif
#ifndef APP_FINGER_POLL_MS
#define APP_FINGER_POLL_MS         200u
#endif

#ifndef APP_STAT_REPORT_MS
#define APP_STAT_REPORT_MS         30000u
#endif
#ifndef APP_STAT_EXPORT_MS
#define APP_STAT_EXPORT_MS         60000u
#endif

#ifndef APP_PROPERTY_POST_MS
#define APP_PROPERTY_POST_MS       5000u
#endif

#ifndef APP_CAP_OFFLINE_Q_DEPTH
#define APP_CAP_OFFLINE_Q_DEPTH    8u
#endif

#endif
