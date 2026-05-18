#include "app_rtos.h"

#if USE_FREERTOS

#include "board_config.h"
#include "wdg.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include "delay.h"
#include "keyboard_sm.h"
#include "lock_manager.h"
#include "dht11.h"
#include "mq2.h"
#include "hc_sr501.h"
#include "gpio.h"
#include "oled_view.h"
#include "oled.h"
#include "esp8266_tls.h"
#include "linkage.h"
#include "capture_task.h"
#include "Capture.h"
#include "app_types.h"
#include "app_params.h"
#include "syslog.h"
#include "as608.h"
#include "usart1.h"

#ifndef STK_LOCK
#define STK_LOCK        512u
#endif
#ifndef STK_SENSOR
#define STK_SENSOR      256u
#endif
#ifndef STK_OLED
#define STK_OLED        256u
#endif
#ifndef STK_NET
#define STK_NET         1024u
#endif
#ifndef STK_LOG
#define STK_LOG         1024u
#endif
#ifndef PRIO_LOCK
#define PRIO_LOCK       5u
#endif
#ifndef PRIO_SENSOR
#define PRIO_SENSOR     3u
#endif
#ifndef PRIO_OLED
#define PRIO_OLED       2u
#endif
#ifndef PRIO_NET
#define PRIO_NET        4u
#endif
#ifndef PRIO_LOG
#define PRIO_LOG        3u
#endif

static sensor_state_t g_sensor;
static esp_state_t g_esp;

static SemaphoreHandle_t s_sensor_mtx;

/* IPC 句柄 */
static QueueHandle_t       s_alarm_q;
static EventGroupHandle_t  s_evt_group;
static SemaphoreHandle_t   s_pir_sem;

extern u8 security_mode;
extern u8 security_alarm;
extern u8 dht11_temp;
extern u8 dht11_humi;
extern u16 mq2_adc_value;
extern arm_mode_t arm_mode;
extern volatile u32 g_false_alarm_count;
extern volatile u32 g_fusion_high_confidence;
extern volatile u8 RELAY_TIME;

extern void CLR_Buf2(void);
extern void Security_Set_ArmMode(arm_mode_t mode);

static void IPC_Init(void)
{
    s_alarm_q   = xQueueCreate(APP_IPC_ALARM_Q_DEPTH, sizeof(alarm_event_t));
    s_evt_group = xEventGroupCreate();
    s_pir_sem   = xSemaphoreCreateBinary();
    /* s_sensor_mtx 仍由 AppRTOS_Start 在 IPC_Init 之后创建 */
}

/* IPC: ISR→Task PIR 通知 */
void IPC_NotifyPIR_FromISR(BaseType_t *pxHigherPriorityTaskWoken)
{
    if (s_pir_sem != NULL)
        xSemaphoreGiveFromISR(s_pir_sem, pxHigherPriorityTaskWoken);
}

/* IPC: EventGroup 通知 —— arm 状态变更 */
void IPC_NotifyArmStateChange(void)
{
    if (s_evt_group != NULL)
        xEventGroupSetBits(s_evt_group, EVT_ARM_STATE);
}

/* IPC: EventGroup 通知 —— 网络上线 */
void IPC_NotifyNetOnline(void)
{
    if (s_evt_group != NULL)
        xEventGroupSetBits(s_evt_group, EVT_NET_ONLINE);
}

/* IPC: EventGroup 通知 —— 抓拍请求 */
void IPC_NotifyCaptureReq(void)
{
    if (s_evt_group != NULL)
        xEventGroupSetBits(s_evt_group, EVT_CAPTURE_REQ);
}

static void Task_Lock(void *arg)
{
    (void)arg;
    for (;;)
    {
        KeyboardSM_Tick();
        LockManager_Tick();
        WDG_Mark(WDG_SRC_ALARM);
        vTaskDelay(pdMS_TO_TICKS(APP_TASK_PERIOD_SECURITY_MS));
    }
}

static void Task_Sensor(void *arg)
{
    (void)arg;
    uint32_t last_dht = 0;
    uint32_t last_mq2 = 0;
    static u8 s_mq2_was_alarm = 0u;
    static uint32_t s_pir_low_since = 0u;
    static uint32_t s_brute_beep_until = 0u;

    for (;;)
    {
        uint32_t now = (uint32_t)xTaskGetTickCount();

        if ((uint32_t)(now - last_dht) >= pdMS_TO_TICKS(APP_DHT_POLL_MS))
        {
            u8 t = 0, h = 0;
            last_dht = now;
            if (DHT11_Read_Data(&t, &h) == 0)
            {
                dht11_temp = t;
                dht11_humi = h;
                xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);
                g_sensor.temp = t;
                g_sensor.humi = h;
                g_sensor.dht_ok = 1u;
                xSemaphoreGive(s_sensor_mtx);
            }
            else
            {
                xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);
                g_sensor.dht_ok = 0u;
                xSemaphoreGive(s_sensor_mtx);
            }
        }

        if ((uint32_t)(now - last_mq2) >= pdMS_TO_TICKS(APP_MQ2_POLL_MS))
        {
            last_mq2 = now;
            mq2_adc_value = MQ2_Read_ADC_Filter();
            mq2_adc_value = MQ2_ApplyTempComp(mq2_adc_value, (int16_t)dht11_temp);
            MQ2_UpdateBaseline(mq2_adc_value);
            xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);
            g_sensor.mq2_adc = mq2_adc_value;
            g_sensor.mq2_alarm = MQ2_Check_Alarm(mq2_adc_value);
            xSemaphoreGive(s_sensor_mtx);
#if BOARD_MQ2_ALARM_ENABLE
            if (g_sensor.mq2_alarm && !s_mq2_was_alarm)
            {
                Linkage_OnGasState(1u);
#if EN_OV7670_LOCAL
                Capture_Request(CAP_EVT_GAS);
                IPC_NotifyCaptureReq();
#endif
            }
            if (!g_sensor.mq2_alarm && s_mq2_was_alarm)
            {
                Linkage_OnGasState(0u);
            }
#endif
            s_mq2_was_alarm = g_sensor.mq2_alarm;
        }

        /* PIR 检测 —— 三段式误触发抑制 (🟡11) + 二进制信号量唤醒 */
        {
            uint32_t now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
            /* 信号量用于即时唤醒，实际判定由 HC_SR501_IsValidTrigger 完成 */
            (void)xSemaphoreTake(s_pir_sem, 0);
            if (HC_SR501_IsValidTrigger(now_ms))
            {
                g_pir_total_triggers++;
                s_pir_low_since = 0u;
                if (arm_mode == ARM_MODE_DISARM)
                {
                    g_false_alarm_count++;
                }
                else
                {
                    g_intrusion_confirm++;
                    xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);
                    g_sensor.pir_alarm = 1u;
                    xSemaphoreGive(s_sensor_mtx);
                }
            }
        }
        if (!HC_SR501_Poll_Triggered())
        {
            if (g_sensor.pir_alarm)
            {
                if (s_pir_low_since == 0u)
                    s_pir_low_since = now;
                if ((uint32_t)(now - s_pir_low_since) >= pdMS_TO_TICKS(APP_PIR_CLEAR_LOW_MS))
                {
                    xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);
                    g_sensor.pir_alarm = 0u;
                    xSemaphoreGive(s_sensor_mtx);
                    s_pir_low_since = 0u;
                }
            }
        }

        /* 告警与执行器处理 */
        {
            alarm_type_t alarm = ALARM_NONE;

            if (security_mode && g_sensor.pir_alarm) alarm = ALARM_PIR;
            if (BOARD_MQ2_ALARM_ENABLE && g_sensor.mq2_alarm)
                alarm = (alarm == ALARM_PIR) ? ALARM_BOTH : ALARM_GAS;

            if (LockManager_IsBruteAlarm())
            {
                Security_Set_ArmMode(ARM_MODE_AWAY);
                s_brute_beep_until = now + pdMS_TO_TICKS(APP_BRUTE_BEEP_MS);
            }
            if ((uint32_t)(now - s_brute_beep_until) < pdMS_TO_TICKS(APP_BRUTE_BEEP_MS))
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

            if (alarm != ALARM_NONE)
            {
                u8 is_pir = (alarm == ALARM_PIR || alarm == ALARM_BOTH);
                u8 is_gas = (alarm == ALARM_GAS || alarm == ALARM_BOTH);

                g_last_alarm_trigger_tick = Bare_GetTickMs();
                g_last_alarm_action_tick = Bare_GetTickMs();

                /* IPC: 告警事件入队 */
                if (s_alarm_q != NULL)
                {
                    alarm_event_t evt;
                    evt.type    = alarm;
                    evt.tick_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
                    evt.adc_value = mq2_adc_value;
                    xQueueSend(s_alarm_q, &evt, 0);
                }

                if (is_pir)
                {
                    if (arm_mode == ARM_MODE_HOME)
                    {
                        BEEP_StartPattern(BEEP_PATTERN_INTRUSION);
                    }
                    else
                    {
                        /* 多传感器融合评分 —— 论文 4.4 */
                        u8 epir = 100u;  /* PIR triggered + debounced */
                        u8 ecam = g_cap_evt_pending ? 50u : 0u;
                        u8 eacc = (alarm == ALARM_BOTH) ? 80u : 0u;

                        if (Fusion_IsHighConfidence(epir, ecam, eacc))
                        {
                            BEEP_StartPattern(BEEP_PATTERN_INTRUSION);
                            Linkage_OnIntrusion();
#if EN_OV7670_LOCAL
                            Capture_Request(CAP_EVT_INTRUSION);
                            IPC_NotifyCaptureReq();
#endif
                            SysLog_Add(LOG_EVT_ALARM, "PIR_INTRUSION");
                            g_fusion_high_confidence++;
                        }
                        else
                        {
                            BEEP_StartPattern(BEEP_PATTERN_DOORBELL);
                            SysLog_Add(LOG_EVT_CONFIG, "PIR_LOW_CONF");
                        }
                    }
                }
                if (is_gas)
                {
                    BEEP_StartPattern(BEEP_PATTERN_GAS);
                    Linkage_OnMQ2_Alarm();
                }
            }
            else
            {
                if (LockManager_IsBruteAlarm())
                    BEEP_StartPattern(BEEP_PATTERN_LOCKOUT);
                else
                    BEEP_Stop();
            }

            OLED_View_ShowAlarm(alarm);
        }

        BEEP_Tick10ms();
        WDG_Mark(WDG_SRC_SENSOR);
        vTaskDelay(pdMS_TO_TICKS(APP_TASK_PERIOD_SENSOR_MS));
    }
}

static void Task_Net(void *arg)
{
    (void)arg;
    uint32_t last_post = 0;
    static u8 s_was_online = 0u;

    for (;;)
    {
        ESP8266_OneNET_InitFsm_Poll();
        g_esp.wifi_ready = tls_inited ? 1u : 0u;
        g_esp.hb_ok = ESP8266_Online_Flag ? 1u : 0u;

        if (ESP8266_Online_Flag)
        {
            uint32_t now = (uint32_t)xTaskGetTickCount();
            if (!s_was_online)
            {
                IPC_NotifyNetOnline();
                s_was_online = 1u;
            }
            OneNET_Parse_Cmd();
            if ((uint32_t)(now - last_post) >= pdMS_TO_TICKS(APP_PROPERTY_POST_MS))
            {
                last_post = now;
                OneNET_Publish_Data(dht11_temp, dht11_humi, mq2_adc_value,
                                    (RELAY == 0) ? 1u : 0u,
                                    security_mode, security_alarm, Ctrl_Led);
                CLR_Buf2();
            }
        }
        else
        {
            s_was_online = 0u;
        }

        WDG_Mark(WDG_SRC_NET);
        vTaskDelay(pdMS_TO_TICKS(APP_TASK_PERIOD_NET_MS));
    }
}

static void Task_OLED(void *arg)
{
    (void)arg;
    for (;;)
    {
        xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);
        OLED_View_RefreshDashboard(&g_sensor, LockManager_GetState(), &g_esp);
        xSemaphoreGive(s_sensor_mtx);
        WDG_Mark(WDG_SRC_UI);
        vTaskDelay(pdMS_TO_TICKS(APP_TASK_PERIOD_OLED_MS));
    }
}

static void Task_Log(void *arg)
{
    (void)arg;
    for (;;)
    {
        WDG_Mark(WDG_SRC_SYSMON);

        /* IPC: 从告警队列取事件并写 syslog */
        if (s_alarm_q != NULL)
        {
            alarm_event_t evt;
            while (xQueueReceive(s_alarm_q, &evt, 0) == pdTRUE)
            {
                if (evt.type == ALARM_PIR)
                    SysLog_Add(LOG_EVT_ALARM, "Q_PIR");
                else if (evt.type == ALARM_GAS)
                    SysLog_Add(LOG_EVT_ALARM, "Q_GAS");
                else if (evt.type == ALARM_BOTH)
                    SysLog_Add(LOG_EVT_ALARM, "Q_BOTH");
            }
        }

        /* 指纹轮询：每 200ms 一次，匹配则解锁 */
#if EN_AS608
        {
            g_finger_total_attempts++;
            unsigned short match = AS608_Find_Fingerprint();
            if (match > 0u && match != (unsigned short)0xFFFEu)
            {
                g_finger_success++;
                RELAY = 0;
                RELAY_TIME = APP_LOCK_OPEN_HOLD_TICKS;
                OLED_ShowString(0, 16, "FP UNLOCK OK    ", 16);
                Linkage_OnUnlock(UNLOCK_SRC_FINGER);
                SysLog_Add(LOG_EVT_CONFIG, "FP_UNLOCK");
                arm_mode = ARM_MODE_DISARM;
                security_mode = 0u;
                LockManager_ClearBruteAlarm();
            }
        }
#endif

        /* 抓拍轮询 — FreeRTOS 环境下替代主循环中的 Bare_CapturePoll */
        Bare_CapturePoll();

        /* 指标6: 统计 uptime (每 200ms 迭代一次，计 5 次=1 秒) */
        {
            static u8 s_uptime_div = 0u;
            if (++s_uptime_div >= 5u)
            {
                s_uptime_div = 0u;
                g_uptime_seconds++;
            }
        }

        WDG_Pump();
        vTaskDelay(pdMS_TO_TICKS(200u));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    SysLog_Add(LOG_EVT_ALARM, "STACK_OVF");
    UART1_SendStr("[FATAL] StackOverflow: ");
    UART1_SendStr((char *)pcTaskName);
    UART1_SendStr("\r\n");
    for (;;)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    SysLog_Add(LOG_EVT_ALARM, "MALLOC_FAIL");
    UART1_SendStr("[FATAL] MallocFailed\r\n");
    for (;;)
    {
    }
}

void AppRTOS_Start(void)
{
    BaseType_t ok;

    IPC_Init();

    s_sensor_mtx = xSemaphoreCreateMutex();
    if (s_sensor_mtx == NULL)
    {
        OLED_ShowString(0u, 16u, "MTX FAIL        ", 16u);
        return;
    }

    ok = xTaskCreate(Task_Lock, "lock", (uint16_t)STK_LOCK, NULL, (UBaseType_t)PRIO_LOCK, NULL);
    if (ok != pdPASS)
    {
        OLED_ShowString(0u, 32u, "T_LOCK FAIL     ", 16u);
        return;
    }
    ok = xTaskCreate(Task_Sensor, "sensor", (uint16_t)STK_SENSOR, NULL, (UBaseType_t)PRIO_SENSOR, NULL);
    if (ok != pdPASS)
    {
        OLED_ShowString(0u, 32u, "T_SENSOR FAIL   ", 16u);
        return;
    }
    ok = xTaskCreate(Task_OLED, "oled", (uint16_t)STK_OLED, NULL, (UBaseType_t)PRIO_OLED, NULL);
    if (ok != pdPASS)
    {
        OLED_ShowString(0u, 32u, "T_OLED FAIL     ", 16u);
        return;
    }
    ok = xTaskCreate(Task_Net, "net", (uint16_t)STK_NET, NULL, (UBaseType_t)PRIO_NET, NULL);
    if (ok != pdPASS)
    {
        OLED_ShowString(0u, 32u, "T_NET FAIL      ", 16u);
        return;
    }
    ok = xTaskCreate(Task_Log, "log", (uint16_t)STK_LOG, NULL, (UBaseType_t)PRIO_LOG, NULL);
    if (ok != pdPASS)
    {
        OLED_ShowString(0u, 32u, "T_LOG FAIL      ", 16u);
        return;
    }

    SysLog_Add(LOG_EVT_CONFIG, "RTOS_START");
    vTaskStartScheduler();

    OLED_ShowString(0u, 16u, "RTOS START FAIL ", 16u);
}

#endif /* USE_FREERTOS */
