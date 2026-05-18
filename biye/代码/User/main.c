#include <stdio.h>
#include <string.h>
#include "board_config.h"
#include "delay.h"
#include "gpio.h"
#include "timer.h"
#include "key4_4.h"
#include "dht11.h"
#include "mq2.h"
#include "hc_sr501.h"
#include "usart1.h"
#include "usart2.h"
#include "ov7670_fifo.h"
#include "capture_task.h"
#include "Capture.h"
#include "keyboard_sm.h"
#include "lock_manager.h"
#include "oled_view.h"
#include "oled.h"
#include "linkage.h"
#include "esp8266_tls.h"
#include "syslog.h"
#include "app_types.h"
#include "app_params.h"
#include "log.h"
#include "app_rtos.h"
#include "wdg.h"

#define BUF2_MAX 800
#define BUF3_MAX 400

volatile u8 RELAY_TIME = 0;
volatile u8 InitDisplay = 1;
volatile u8 pass = 0;
volatile u8 ReInputEn = 0;

u8 security_mode = 0;
u8 security_alarm = 0;
security_fsm_state_t g_security_state = SEC_DISARMED;  /* 🟡6: 安防FSM状态 */
u8 ui_busy = 0;
arm_mode_t arm_mode = ARM_MODE_DISARM;

u8 dht11_temp = 0;
u8 dht11_humi = 0;
u8 dht11_data_valid = 0;
u16 mq2_adc_value = 0;

char Uart2_Buf[BUF2_MAX];
char Uart3_Buf[BUF3_MAX];

static volatile u16 s_uart2_wr = 0;
static volatile u16 s_uart3_wr = 0;

static sensor_state_t g_sensor;
static esp_state_t g_esp;
extern volatile u32 g_net_reconnect_count;
extern volatile u32 g_net_offline_count;
extern volatile u16 g_cap_replay_count;
extern volatile u16 g_cap_sd_write_fail_count;
volatile unsigned char g_as608_user_abort = 0u;
volatile u32 g_false_alarm_count = 0u;
static volatile u32 g_runtime_exception_count = 0u;

/* ---------- 论文第六章性能指标统计计数器 ---------- */

/* 指标1: 门禁识别准确率 */
volatile u32 g_pwd_total_attempts = 0;    // total password attempts
volatile u32 g_pwd_success = 0;           // successful password unlocks
volatile u32 g_finger_total_attempts = 0; // total fingerprint attempts
volatile u32 g_finger_success = 0;        // successful fingerprint unlocks

/* 指标2: 入侵检测误报率 */
volatile u32 g_pir_total_triggers = 0;    // all PIR triggers (armed + disarmed)
volatile u32 g_intrusion_confirm = 0;     // confirmed intrusions (armed mode)

	/* 多传感器融合高置信度判定次数 (论文 4.4) */
	volatile u32 g_fusion_high_confidence = 0u;

/* 指标3: 本地告警响应时延 */
volatile u32 g_last_alarm_trigger_tick = 0;  // PIR trigger timestamp (ms)
volatile u32 g_last_alarm_action_tick = 0;   // buzzer on timestamp (ms)

/* 指标4: 远程控制响应时延 */
volatile u32 g_last_cmd_received_tick = 0;   // MQTT command receive timestamp
volatile u32 g_last_cmd_completed_tick = 0;  // command execution complete timestamp

/* 指标5: 抓拍写入成功率 */
volatile u16 g_cap_total_attempts = 0;   // total capture attempts
volatile u16 g_cap_success_count = 0;    // successful SD writes

/* 指标6: 连续运行 */
volatile u32 g_uptime_seconds = 0;       // system uptime in seconds

void AS608_PollYield(void)
{
    KeyboardSM_Tick();
}

void CLR_Buf2(void)
{
    memset(Uart2_Buf, 0, sizeof(Uart2_Buf));
    s_uart2_wr = 0;
}

void CLR_Buf(void)
{
    memset(Uart3_Buf, 0, sizeof(Uart3_Buf));
    s_uart3_wr = 0;
}

u8 Find2(char *a)
{
    if(a == NULL) return 0;
    return (strstr(Uart2_Buf, a) != NULL) ? 1u : 0u;
}

u8 Find(char *a)
{
    if(a == NULL) return 0;
    return (strstr(Uart3_Buf, a) != NULL) ? 1u : 0u;
}

void Security_Set_Mode(u8 armed)
{
    security_mode = armed ? 1u : 0u;
    arm_mode = armed ? ARM_MODE_AWAY : ARM_MODE_DISARM;
#if USE_FREERTOS
    IPC_NotifyArmStateChange();
#endif
}

void Security_Set_ArmMode(arm_mode_t mode)
{
    arm_mode = mode;
    security_mode = (mode != ARM_MODE_DISARM) ? 1u : 0u;
#if USE_FREERTOS
    IPC_NotifyArmStateChange();
#endif
}

void ESP8266_CooperativeYield(void)
{
    KeyboardSM_Tick();
}

void USART3_IRQHandler(void)
{
    if(USART_GetITStatus(BOARD_USART3_INSTANCE, USART_IT_RXNE) != RESET)
    {
        char c = (char)USART_ReceiveData(BOARD_USART3_INSTANCE);

        if(s_uart2_wr < (u16)(BUF2_MAX - 1u))
        {
            Uart2_Buf[s_uart2_wr++] = c;
            Uart2_Buf[s_uart2_wr] = '\0';
        }
        if(s_uart3_wr < (u16)(BUF3_MAX - 1u))
        {
            Uart3_Buf[s_uart3_wr++] = c;
            Uart3_Buf[s_uart3_wr] = '\0';
        }
    }
}

static void app_init(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init();

    ESP8266_EN_GPIO_Init();

    uart1_Init(57600);
    USART2_Init_Config(BOARD_WIFI_UART_BAUD);

    BEEP_AND_RELAY_GPIO_Init();
    Actuator_EnterSafeState();
    Key_Init();
    TIM2_Init(7199, 99);
    HC_SR501_Init();
    HC_SR501_EXTI_Init();
    DHT11_Init();
    MQ2_Init();

#if EN_OV7670_LOCAL
    OV7670_FIFO_Init();
    Capture_Task_Create();
#endif

    OLED_View_Init();
    OLED_View_ShowBootSelfTest();
    LockManager_Init();
    KeyboardSM_Init();
    Linkage_Init();
    SysLog_Init();
    SysLog_Add(LOG_EVT_CONFIG, "BOOT");
    if(WDG_LastResetWasIWDG())
    {
        SysLog_Add(LOG_EVT_ALARM, "RST_IWDG");
    }
    WDG_Init();
    WDG_HwInit();

    OLED_View_RefreshDashboard(&g_sensor, LockManager_GetState(), &g_esp);
}

#if !USE_FREERTOS
static void app_poll_sensors(void)
{
    static u32 s_dht_ms = 0;
    static u32 s_mq2_ms = 0;
    static u32 s_pir_low_since = 0u;
    static u8 s_dht_was_ok = 1u;
    static u8 s_dht_fail_streak = 0u;
    static u8 s_dht_ok_streak = 0u;
    static u32 s_dht_backoff_ms = APP_DHT_POLL_MS;
    static u8 s_mq2_was_alarm = 0u;
    static u16 s_mq2_last_adc = 0u;
    static u8 s_mq2_stuck_cnt = 0u;
    static u8 s_mq2_recover_cnt = 0u;
    static u8 s_mq2_fault = 0u;
    u32 now = Bare_GetTickMs();

    if((u32)(now - s_dht_ms) >= s_dht_backoff_ms)
    {
        u8 t = 0, h = 0;
        s_dht_ms = now;
        if(DHT11_Read_Data(&t, &h) == 0)
        {
            s_dht_fail_streak = 0u;
            if(s_dht_ok_streak < 0xFFu) s_dht_ok_streak++;
            dht11_temp = t;
            dht11_humi = h;
            dht11_data_valid = 1u;
            g_sensor.temp = t;
            g_sensor.humi = h;
            if(s_dht_ok_streak >= APP_DHT_RECOVER_SUCCESSES)
            {
                g_sensor.dht_ok = 1u;
                s_dht_backoff_ms = APP_DHT_POLL_MS;
            }
        }
        else
        {
            s_dht_ok_streak = 0u;
            if(s_dht_fail_streak < 0xFFu) s_dht_fail_streak++;
            dht11_data_valid = 0u;
            if(s_dht_fail_streak >= APP_DHT_OFFLINE_FAILS)
            {
                g_sensor.dht_ok = 0u;
                if(s_dht_was_ok)
                {
                    LOG_SENSOR("DHT offline");
                    SysLog_Add(LOG_EVT_ALARM, "DHT_OFFLINE");
                }
                if(s_dht_backoff_ms < APP_DHT_RETRY_BACKOFF_MAX_MS)
                {
                    s_dht_backoff_ms <<= 1;
                    if(s_dht_backoff_ms > APP_DHT_RETRY_BACKOFF_MAX_MS)
                        s_dht_backoff_ms = APP_DHT_RETRY_BACKOFF_MAX_MS;
                }
            }
        }
        if(g_sensor.dht_ok && !s_dht_was_ok)
        {
            LOG_SENSOR("DHT recover");
            SysLog_Add(LOG_EVT_CONFIG, "DHT_RECOVER");
        }
        s_dht_was_ok = g_sensor.dht_ok;
    }

    if((u32)(now - s_mq2_ms) >= (u32)APP_MQ2_POLL_MS)
    {
        s_mq2_ms = now;
        mq2_adc_value = MQ2_Read_ADC_Filter();
        mq2_adc_value = MQ2_ApplyTempComp(mq2_adc_value, (int16_t)dht11_temp);
        MQ2_UpdateBaseline(mq2_adc_value);
        g_sensor.mq2_adc = mq2_adc_value;
        if(mq2_adc_value > 4095u)
        {
            s_mq2_fault = 1u;
        }
        else
        {
            u16 diff = (mq2_adc_value > s_mq2_last_adc) ? (mq2_adc_value - s_mq2_last_adc) : (s_mq2_last_adc - mq2_adc_value);
            if(diff <= APP_MQ2_STUCK_DIFF_ADC)
            {
                if(s_mq2_stuck_cnt < 0xFFu) s_mq2_stuck_cnt++;
            }
            else
            {
                s_mq2_stuck_cnt = 0u;
                if(s_mq2_fault)
                {
                    if(s_mq2_recover_cnt < 0xFFu) s_mq2_recover_cnt++;
                    if(s_mq2_recover_cnt >= APP_MQ2_RECOVER_GOOD_COUNT)
                    {
                        s_mq2_fault = 0u;
                        s_mq2_recover_cnt = 0u;
                        SysLog_Add(LOG_EVT_CONFIG, "MQ2_RECOVER");
                        LOG_SENSOR("MQ2 recover");
                    }
                }
            }
            if(s_mq2_stuck_cnt >= APP_MQ2_STUCK_COUNT)
            {
                if(!s_mq2_fault)
                {
                    SysLog_Add(LOG_EVT_ALARM, "MQ2_FAULT");
                    LOG_SENSOR("MQ2 fault");
                }
                s_mq2_fault = 1u;
                s_mq2_recover_cnt = 0u;
            }
        }
        s_mq2_last_adc = mq2_adc_value;
        g_sensor.mq2_alarm = s_mq2_fault ? 0u : MQ2_Check_Alarm(mq2_adc_value);
        if(g_sensor.mq2_alarm && !s_mq2_was_alarm)
        {
#if BOARD_MQ2_ALARM_ENABLE
            Linkage_OnGasState(1u);
#if EN_OV7670_LOCAL
            Capture_Request(CAP_EVT_GAS);
#endif
#endif
            LOG_SENSOR("MQ2 alarm on");
        }
        if(!g_sensor.mq2_alarm && s_mq2_was_alarm)
        {
#if BOARD_MQ2_ALARM_ENABLE
            Linkage_OnGasState(0u);
#endif
            LOG_SENSOR("MQ2 alarm clear");
        }
        s_mq2_was_alarm = g_sensor.mq2_alarm;
    }

    /* 🟡11: 三段式误触发抑制 */
    {
        u8 irq_flag = HC_SR501_IRQHandler_GetFlag();
        if(irq_flag) HC_SR501_IRQHandler_ClearFlag();
        (void)irq_flag; /* EXTI 仅作唤醒，判定由 IsValidTrigger 完成 */
        if(HC_SR501_IsValidTrigger(now))
        {
            g_pir_total_triggers++;
            s_pir_low_since = 0u;
            if(arm_mode == ARM_MODE_DISARM)
            {
                g_false_alarm_count++;
                SysLog_Add(LOG_EVT_CONFIG, "PIR_FALSE_ALARM");
                LOG_SENSOR("PIR suppressed (disarmed)");
            }
            else
            {
                g_intrusion_confirm++;
                g_sensor.pir_alarm = 1u;
            }
        }
    }
    if(!HC_SR501_Poll_Triggered())
    {
        if(g_sensor.pir_alarm)
        {
            if(s_pir_low_since == 0u)
                s_pir_low_since = now;
            if((u32)(now - s_pir_low_since) >= (u32)APP_PIR_CLEAR_LOW_MS)
            {
                g_sensor.pir_alarm = 0u;
                s_pir_low_since = 0u;
            }
        }
    }
}

static void app_report_stats(void)
{
    static u32 s_last_ms = 0u;
    u32 now = Bare_GetTickMs();
    if((u32)(now - s_last_ms) < (u32)APP_STAT_REPORT_MS) return;
    s_last_ms = now;
    LOG_STAT("false_alarm=%lu reconnect=%lu replay=%u sd_fail=%u run_ex=%lu "
             "pwd_att=%lu pwd_ok=%lu fp_att=%lu fp_ok=%lu "
             "pir_tot=%lu intr_ok=%lu cap_att=%u cap_ok=%u cap_vfail=%u cap_q_drop=%u cap_q_ok=%u cap_q_fail=%u "
             "alarm_lat=%lu cmd_lat=%lu uptime=%lu",
             (unsigned long)g_false_alarm_count,
             (unsigned long)g_net_reconnect_count,
             (unsigned)g_cap_replay_count,
             (unsigned)g_cap_sd_write_fail_count,
             (unsigned long)g_runtime_exception_count,
             (unsigned long)g_pwd_total_attempts,
             (unsigned long)g_pwd_success,
             (unsigned long)g_finger_total_attempts,
             (unsigned long)g_finger_success,
             (unsigned long)g_pir_total_triggers,
             (unsigned long)g_intrusion_confirm,
             (unsigned)g_cap_total_attempts,
             (unsigned)g_cap_success_count,
             (unsigned)g_cap_validation_fail_count,
             (unsigned)g_cap_q_drop,
             (unsigned)g_cap_q_flush_ok,
             (unsigned)g_cap_q_flush_fail,
             (unsigned long)(g_last_alarm_action_tick > g_last_alarm_trigger_tick
                ? g_last_alarm_action_tick - g_last_alarm_trigger_tick : 0lu),
             (unsigned long)(g_last_cmd_completed_tick > g_last_cmd_received_tick
                ? g_last_cmd_completed_tick - g_last_cmd_received_tick : 0lu),
             (unsigned long)g_uptime_seconds);
}

static void app_export_stats(void)
{
    static u32 s_last_ms = 0u;
    char line[512];
    u32 now = Bare_GetTickMs();
    if((u32)(now - s_last_ms) < (u32)APP_STAT_EXPORT_MS) return;
    s_last_ms = now;

    sprintf(line,
            "{\"tag\":\"STAT_EXPORT\",\"tick\":%lu"
            ",\"false_alarm\":%lu,\"reconnect\":%lu,\"replay\":%u,\"sd_fail\":%u,\"run_ex_72h\":%lu"
            ",\"pwd_att\":%lu,\"pwd_ok\":%lu"
            ",\"fp_att\":%lu,\"fp_ok\":%lu"
            ",\"pir_tot\":%lu,\"intr_ok\":%lu"
            ",\"cap_att\":%u,\"cap_ok\":%u,\"cap_vfail\":%u,\"cap_q_drop\":%u,\"cap_q_ok\":%u,\"cap_q_fail\":%u"
            ",\"alarm_lat_ms\":%lu,\"cmd_lat_ms\":%lu"
            ",\"uptime_s\":%lu"
            "}",
            (unsigned long)now,
            (unsigned long)g_false_alarm_count,
            (unsigned long)g_net_reconnect_count,
            (unsigned)g_cap_replay_count,
            (unsigned)g_cap_sd_write_fail_count,
            (unsigned long)g_runtime_exception_count,
            (unsigned long)g_pwd_total_attempts,
            (unsigned long)g_pwd_success,
            (unsigned long)g_finger_total_attempts,
            (unsigned long)g_finger_success,
            (unsigned long)g_pir_total_triggers,
            (unsigned long)g_intrusion_confirm,
            (unsigned)g_cap_total_attempts,
            (unsigned)g_cap_success_count,
            (unsigned)g_cap_validation_fail_count,
            (unsigned)g_cap_q_drop,
            (unsigned)g_cap_q_flush_ok,
            (unsigned)g_cap_q_flush_fail,
            (unsigned long)(g_last_alarm_action_tick > g_last_alarm_trigger_tick
                ? g_last_alarm_action_tick - g_last_alarm_trigger_tick : 0lu),
            (unsigned long)(g_last_cmd_completed_tick > g_last_cmd_received_tick
                ? g_last_cmd_completed_tick - g_last_cmd_received_tick : 0lu),
            (unsigned long)g_uptime_seconds);
    uart1_SendStr(line);
    uart1_SendStr("\r\n");
}

static void app_poll_alarm_and_act(void)
{
    alarm_type_t alarm = ALARM_NONE;
    static u32 s_brute_beep_until = 0u;
    u32 now = Bare_GetTickMs();

    /* PIR: HOME=本地蜂鸣+OLED, AWAY=联动+抓拍+云端 */
    if(security_mode && g_sensor.pir_alarm)
    {
        alarm = ALARM_PIR;
    }
    if(BOARD_MQ2_ALARM_ENABLE && g_sensor.mq2_alarm)
        alarm = (alarm == ALARM_PIR) ? ALARM_BOTH : ALARM_GAS;

    /* 暴力破解告警：强制 AWAY 模式 + 蜂鸣 + OneNET 告警 */
    if(LockManager_IsBruteAlarm())
    {
        Security_Set_ArmMode(ARM_MODE_AWAY);
        s_brute_beep_until = now + (u32)APP_BRUTE_BEEP_MS;
    }
    if((u32)(now - s_brute_beep_until) < (u32)APP_BRUTE_BEEP_MS)
        BEEP_StartPattern(BEEP_PATTERN_LOCKOUT);

    /* security_alarm: HOME 模式 PIR 不上报云端 */
    {
        u8 cloud_alarm = 0u;
        if(security_mode && g_sensor.pir_alarm && arm_mode == ARM_MODE_AWAY)
            cloud_alarm = 1u;
        if(BOARD_MQ2_ALARM_ENABLE && g_sensor.mq2_alarm)
            cloud_alarm = 1u;
        security_alarm = cloud_alarm;
    }

    if(alarm != ALARM_NONE)
    {
        u8 is_pir = (alarm == ALARM_PIR || alarm == ALARM_BOTH);
        u8 is_gas = (alarm == ALARM_GAS || alarm == ALARM_BOTH);

        g_last_alarm_trigger_tick = Bare_GetTickMs();
        g_last_alarm_action_tick = Bare_GetTickMs();

        if(is_pir)
        {
            if(arm_mode == ARM_MODE_HOME)
            {
                /* HOME 模式：仅本地蜂鸣+OLED，不上传云端/不联动/不抓拍 */
                BEEP_StartPattern(BEEP_PATTERN_INTRUSION);
            }
            else
            {
                /* Multi-sensor fusion scoring - thesis sec 4.4 */
                u8 epir = 100u;
                u8 ecam = g_cap_last_ok ? 60u : (g_cap_evt_pending ? 30u : 0u); /* 🟡12: 结果驱动 */
                u8 eacc = (alarm == ALARM_BOTH) ? 80u : 0u;

                if (Fusion_IsHighConfidence(epir, ecam, eacc))
                {
                    /* AWAY: full intrusion chain */
                    BEEP_StartPattern(BEEP_PATTERN_INTRUSION);
                    Linkage_OnIntrusion();
#if EN_OV7670_LOCAL
                    Capture_Request(CAP_EVT_INTRUSION);
#endif
                    SysLog_Add(LOG_EVT_ALARM, "PIR_INTRUSION");
                    g_fusion_high_confidence++;
                }
                else
                {
                    /* Low confidence: doorbell beep only, no linkage/capture */
                    BEEP_StartPattern(BEEP_PATTERN_DOORBELL);
                    SysLog_Add(LOG_EVT_CONFIG, "PIR_LOW_CONF");
                }
            }
        }
        if(is_gas)
        {
            BEEP_StartPattern(BEEP_PATTERN_GAS);
            Linkage_OnMQ2_Alarm();
        }
    }
    else
    {
        if(LockManager_IsBruteAlarm())
            BEEP_StartPattern(BEEP_PATTERN_LOCKOUT);
        else
            BEEP_Stop();
    }

    OLED_View_ShowAlarm(alarm);

    /* 🟡6: 安防FSM状态更新 */
    if (security_alarm)
        g_security_state = SEC_ALARMING;
    else if (alarm != ALARM_NONE)
        g_security_state = SEC_TRIGGERED;
    else if (security_mode)
        g_security_state = SEC_ARMED;
    else
        g_security_state = SEC_DISARMED;
}

static void app_poll_net(void)
{
    static u8 s_last_online = 0u;
    static u32 s_last_reconnect_cnt = 0u;
    static u32 s_last_offline_cnt = 0u;
#if EN_ESP8266_ONENET
    ESP8266_OneNET_InitFsm_Poll();
    g_esp.wifi_ready = tls_inited ? 1u : 0u;
    g_esp.hb_ok = ESP8266_Online_Flag ? 1u : 0u;
    if(ESP8266_Online_Flag && !s_last_online)
    {
        LOG_NET("online");
        SysLog_Add(LOG_EVT_CONFIG, "NET_RECOVER");
    }
    if(!ESP8266_Online_Flag && s_last_online)
    {
        LOG_NET("offline");
        SysLog_Add(LOG_EVT_ALARM, "NET_OFFLINE");
        g_runtime_exception_count++;
    }
    if(g_net_reconnect_count != s_last_reconnect_cnt)
    {
        char msg[48];
        sprintf(msg, "NET_RECONN_%lu", (unsigned long)g_net_reconnect_count);
        SysLog_Add(LOG_EVT_CONFIG, msg);
        s_last_reconnect_cnt = g_net_reconnect_count;
    }
    if(g_net_offline_count != s_last_offline_cnt)
    {
        char msg[48];
        sprintf(msg, "NET_OFF_%lu", (unsigned long)g_net_offline_count);
        SysLog_Add(LOG_EVT_ALARM, msg);
        s_last_offline_cnt = g_net_offline_count;
    }
    s_last_online = ESP8266_Online_Flag ? 1u : 0u;
    if(ESP8266_Online_Flag)
    {
        OneNET_Parse_Cmd();
        OneNET_Publish_Data(dht11_temp, dht11_humi, mq2_adc_value,
                            (RELAY == 0) ? 1u : 0u,
                            security_mode, security_alarm, Ctrl_Led);
        CLR_Buf2();
    }
#else
    g_esp.wifi_ready = 0u;
    g_esp.hb_ok = 0u;
    s_last_online = 0u;
#endif
}

static void app_dbg_mark(char c)
{
    OLED_ShowChar(120, 48, c, 16);
}
#endif

int main(void)
{
    app_init();
    Security_Set_Mode(0);

#if USE_FREERTOS
    AppRTOS_Start();
    while(1) {}
#else
    while(1)
    {
        app_dbg_mark('Z');
        app_dbg_mark('K'); KeyboardSM_Tick();
        app_dbg_mark('L'); LockManager_Tick();
#if EN_AS608
        app_dbg_mark('F');
        {
            static u32 fp_next_ms = 0;
            u32 now = Bare_GetTickMs();
            if ((s32)(now - fp_next_ms) >= 0)
            {
                fp_next_ms = now + (u32)APP_FINGER_POLL_MS;
                {
                    g_finger_total_attempts++;
                    unsigned short match = AS608_Find_Fingerprint();
                    if (match > 0u && match != (unsigned short)0xFFFEu)
                    {
                        g_finger_success++;
                        AS608_ClearLockout();
                        RELAY = 0;
                        RELAY_TIME = APP_LOCK_OPEN_HOLD_TICKS;
                        OLED_ShowString(0, 16, "FP UNLOCK OK    ", 16);
                        Linkage_OnUnlock(UNLOCK_SRC_FINGER);
                        SysLog_Add(LOG_EVT_CONFIG, "FP_UNLOCK");
                        arm_mode = ARM_MODE_DISARM;
                        security_mode = 0u;
                        LockManager_ClearBruteAlarm();
                    }
                    else
                    {
                        AS608_RecordFailedAttempt();
                    }
                }
            }
        }
#endif
        app_dbg_mark('S'); app_poll_sensors();
        WDG_Mark(WDG_SRC_SENSOR);
        BEEP_Tick10ms();
        app_dbg_mark('A'); app_poll_alarm_and_act();
        WDG_Mark(WDG_SRC_ALARM);
        app_dbg_mark('N'); app_poll_net();
        WDG_Mark(WDG_SRC_NET);
        app_dbg_mark('C'); Bare_CapturePoll();
        app_dbg_mark('R'); app_report_stats();
        app_dbg_mark('X'); app_export_stats();
        app_dbg_mark('O'); OLED_View_RefreshDashboard(&g_sensor, LockManager_GetState(), &g_esp);
        WDG_Mark(WDG_SRC_UI);
        WDG_Mark(WDG_SRC_SYSMON);
        WDG_Pump();
        /* 指标6: 统计 uptime (主循环 20ms, 50次=1秒) */
        {
            static u8 s_uptime_div = 0u;
            if (++s_uptime_div >= 50u)
            {
                s_uptime_div = 0u;
                g_uptime_seconds++;
            }
        }
        delay_ms(20);
    }
#endif
}
