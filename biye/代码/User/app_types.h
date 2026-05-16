#ifndef __APP_TYPES_H
#define __APP_TYPES_H

#include "sys.h"

typedef enum
{
    USER_NONE = 0,
    USER_NORMAL = 1,
    USER_ADMIN = 2
} user_level_t;

typedef enum
{
    ALARM_NONE = 0,
    ALARM_PIR = 1,
    ALARM_GAS = 2,
    ALARM_BOTH = 3
} alarm_type_t;

typedef struct
{
    u8 digits[6];
    u8 count;
    u8 active;
} keypad_input_t;

typedef struct
{
    u8 temp;
    u8 humi;
    u16 mq2_adc;
    u8 mq2_alarm;
    u8 dht_ok;
    u8 pir_alarm;
} sensor_state_t;

typedef struct
{
    user_level_t user_level;
    u8 armed;
    u8 relay_on;
    u8 pwd_failed_count;
    u8 pwd_locked;
    u32 pwd_lock_until_ms;
    u32 admin_until_ms;
    u8 pwd_chg_state;
    u8 enroll_pending;
} lock_state_t;

typedef struct
{
    u8 wifi_ready;
    u8 hb_ok;
    u8 remote_cmd_pending;
    u8 remote_unlock_req;
    u8 remote_arm_req;
    u8 remote_disarm_req;
} esp_state_t;

#endif
