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
    ARM_MODE_DISARM = 0,
    ARM_MODE_HOME   = 1,
    ARM_MODE_AWAY   = 2
} arm_mode_t;

typedef enum
{
    ALARM_NONE = 0,
    ALARM_PIR = 1,
    ALARM_GAS = 2,
    ALARM_BOTH = 3
} alarm_type_t;

/* 🟡5: 门禁五态 FSM — 论文 §5.1 IDLE→ACQUIRE→MATCH→OPEN→LOCKOUT */
typedef enum
{
    DOOR_IDLE    = 0,  /* 空闲，等待输入 */
    DOOR_ACQUIRE = 1,  /* 采集密码/指纹 */
    DOOR_MATCH   = 2,  /* 比对中 */
    DOOR_OPEN     = 3,  /* 门锁打开（含管理员态） */
    DOOR_LOCKOUT  = 4   /* 锁定（连续失败/暴力告警） */
} door_fsm_state_t;

/* 🟡6: 安防五态 FSM — 论文 §4.1 DISARMED→ARMED→TRIGGERED→ALARMING→RECOVERY */
typedef enum
{
    SEC_DISARMED  = 0,  /* 撤防 */
    SEC_ARMED     = 1,  /* 布防（HOME 或 AWAY） */
    SEC_TRIGGERED = 2,  /* 传感器触发，入口延迟/复核 */
    SEC_ALARMING  = 3,  /* 确认告警，蜂鸣+抓拍+上报 */
    SEC_RECOVERY  = 4   /* 告警解除，恢复中 */
} security_fsm_state_t;

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
    door_fsm_state_t state;   /* 🟡5: 形式化 FSM 状态 */
    u8 armed;
    u8 relay_on;
    u8 pwd_failed_count;
    u8 pwd_locked;
    u32 pwd_lock_until_ms;
    u32 admin_until_ms;
    u8 pwd_chg_state;
    u8 enroll_pending;
    u8 brute_alarm;
    u8 brute_alarm_sent;
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

/* ---------- IPC: 告警队列元素 ---------- */
typedef struct
{
    alarm_type_t type;
    uint32_t     tick_ms;
    uint16_t     adc_value;
} alarm_event_t;

/* ---------- IPC: 事件组位定义 ---------- */
#define EVT_ARM_STATE    (1u << 0)
#define EVT_NET_ONLINE   (1u << 1)
#define EVT_CAPTURE_REQ  (1u << 2)

/* ---------- 论文第六章性能指标统计 extern ---------- */
extern volatile u32 g_pwd_total_attempts, g_pwd_success;
extern volatile u32 g_finger_total_attempts, g_finger_success;
extern volatile u32 g_pir_total_triggers, g_intrusion_confirm;
extern volatile u32 g_last_alarm_trigger_tick, g_last_alarm_action_tick;
extern volatile u32 g_last_cmd_received_tick, g_last_cmd_completed_tick;
extern volatile u16 g_cap_total_attempts, g_cap_success_count;
extern volatile u32 g_uptime_seconds;

#endif
