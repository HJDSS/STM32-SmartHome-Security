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
