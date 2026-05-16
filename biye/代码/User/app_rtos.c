#include "app_rtos.h"

#if USE_FREERTOS

#include "board_config.h"
#include "wdg.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

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
#include "app_types.h"
#include "app_params.h"
#include "syslog.h"

#ifndef STK_LOCK
#define STK_LOCK        512u
#endif
#ifndef STK_SENSOR
#define STK_SENSOR      384u
#endif
#ifndef STK_OLED
#define STK_OLED        256u
#endif
#ifndef STK_NET
#define STK_NET         640u
#endif
#ifndef STK_LOG
#define STK_LOG         320u
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
#define PRIO_LOG        1u
#endif

static sensor_state_t g_sensor;
static esp_state_t g_esp;

static SemaphoreHandle_t s_sensor_mtx;

extern u8 security_mode;
extern u8 security_alarm;
extern u8 dht11_temp;
extern u8 dht11_humi;
extern u16 mq2_adc_value;

extern void CLR_Buf2(void);

static void Task_Lock(void *arg)
{
    (void)arg;
    for (;;)
    {
        KeyboardSM_Tick();
        LockManager_Tick();
        WDG_Mark(WDG_SRC_ALARM);
        vTaskDelay(pdMS_TO_TICKS(20u));
    }
}

static void Task_Sensor(void *arg)
{
    (void)arg;
    uint32_t last_dht = 0;
    uint32_t last_mq2 = 0;
    static u8 s_mq2_was_alarm = 0u;

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
                g_sensor.temp = t;
                g_sensor.humi = h;
                g_sensor.dht_ok = 1u;
            }
            else
            {
                g_sensor.dht_ok = 0u;
            }
        }

        if ((uint32_t)(now - last_mq2) >= pdMS_TO_TICKS(APP_MQ2_POLL_MS))
        {
            last_mq2 = now;
            mq2_adc_value = MQ2_Read_ADC_Filter();
            g_sensor.mq2_adc = mq2_adc_value;
            g_sensor.mq2_alarm = MQ2_Check_Alarm(mq2_adc_value);
#if BOARD_MQ2_ALARM_ENABLE
            if (g_sensor.mq2_alarm && !s_mq2_was_alarm)
            {
                Linkage_OnGasState(1u);
#if EN_OV7670_LOCAL
                Capture_Request(CAP_EVT_GAS);
#endif
            }
            if (!g_sensor.mq2_alarm && s_mq2_was_alarm)
            {
                Linkage_OnGasState(0u);
            }
#endif
            s_mq2_was_alarm = g_sensor.mq2_alarm;
        }

        if (HC_SR501_IRQHandler_GetFlag())
        {
            HC_SR501_IRQHandler_ClearFlag();
            g_sensor.pir_alarm = 1u;
        }
        else if (!HC_SR501_Poll_Triggered())
        {
            g_sensor.pir_alarm = 0u;
        }

        WDG_Mark(WDG_SRC_SENSOR);
        vTaskDelay(pdMS_TO_TICKS(10u));
    }
}

static void Task_Net(void *arg)
{
    (void)arg;
    uint32_t last_post = 0;

    for (;;)
    {
        ESP8266_OneNET_InitFsm_Poll();
        g_esp.wifi_ready = tls_inited ? 1u : 0u;
        g_esp.hb_ok = ESP8266_Online_Flag ? 1u : 0u;

        if (ESP8266_Online_Flag)
        {
            uint32_t now = (uint32_t)xTaskGetTickCount();
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

        WDG_Mark(WDG_SRC_NET);
        vTaskDelay(pdMS_TO_TICKS(50u));
    }
}

static void Task_OLED(void *arg)
{
    (void)arg;
    for (;;)
    {
        OLED_View_RefreshDashboard(&g_sensor, LockManager_GetState(), &g_esp);
        WDG_Mark(WDG_SRC_UI);
        vTaskDelay(pdMS_TO_TICKS(100u));
    }
}

static void Task_Log(void *arg)
{
    (void)arg;
    for (;;)
    {
        WDG_Mark(WDG_SRC_SYSMON);
        WDG_Pump();
        vTaskDelay(pdMS_TO_TICKS(1000u));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for (;;)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    for (;;)
    {
    }
}

void AppRTOS_Start(void)
{
    BaseType_t ok;

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
