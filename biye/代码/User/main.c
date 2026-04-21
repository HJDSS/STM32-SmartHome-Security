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
#include "linkage.h"
#include "esp8266_tls.h"
#include "syslog.h"
#include "app_types.h"

#define BUF2_MAX 800
#define BUF3_MAX 400

volatile u8 RELAY_TIME = 0;
volatile u8 InitDisplay = 1;
volatile u8 pass = 0;
volatile u8 ReInputEn = 0;

u8 security_mode = 0;
u8 security_alarm = 0;
u8 ui_busy = 0;

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
    uart1_Init(115200);
    USART2_Init_Config(BOARD_WIFI_UART_BAUD);

    BEEP_AND_RELAY_GPIO_Init();
    RELAY = 1;
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
}

static void app_poll_sensors(void)
{
    static u32 s_dht_ms = 0;
    static u32 s_mq2_ms = 0;
    static u32 s_pir_quiet_until = 0;
    u32 now = Bare_GetTickMs();

    if((u32)(now - s_dht_ms) >= 2000u)
    {
        u8 t = 0, h = 0;
        s_dht_ms = now;
        if(DHT11_Read_Data(&t, &h) == 0)
        {
            dht11_temp = t;
            dht11_humi = h;
            dht11_data_valid = 1u;
            g_sensor.temp = t;
            g_sensor.humi = h;
            g_sensor.dht_ok = 1u;
        }
        else
        {
            dht11_data_valid = 0u;
            g_sensor.dht_ok = 0u;
        }
    }

    if((u32)(now - s_mq2_ms) >= 250u)
    {
        s_mq2_ms = now;
        mq2_adc_value = MQ2_Read_ADC_Filter();
        g_sensor.mq2_adc = mq2_adc_value;
        g_sensor.mq2_alarm = MQ2_Check_Alarm(mq2_adc_value);
    }

    if(HC_SR501_IRQHandler_GetFlag())
    {
        HC_SR501_IRQHandler_ClearFlag();
        if((u32)(now - s_pir_quiet_until) < 3000000000u)
        {
            g_sensor.pir_alarm = 1u;
        }
        else
        {
            s_pir_quiet_until = now + 3000u;
        }
    }
    else if(!HC_SR501_Poll_Triggered())
    {
        g_sensor.pir_alarm = 0u;
    }
}

static void app_poll_alarm_and_act(void)
{
    alarm_type_t alarm = ALARM_NONE;

    if(security_mode && g_sensor.pir_alarm) alarm = ALARM_PIR;
    if(BOARD_MQ2_ALARM_ENABLE && g_sensor.mq2_alarm)
        alarm = (alarm == ALARM_PIR) ? ALARM_BOTH : ALARM_GAS;

    security_alarm = (alarm != ALARM_NONE) ? 1u : 0u;

    if(alarm != ALARM_NONE)
    {
        BEEP_SoundOn();
        if(alarm == ALARM_PIR || alarm == ALARM_BOTH)
            Linkage_OnIntrusion();
        if(alarm == ALARM_GAS || alarm == ALARM_BOTH)
            Linkage_OnMQ2_Alarm();
#if EN_OV7670_LOCAL
        Capture_Request(CAP_EVT_PIR);
#endif
    }
    else
    {
        BEEP_SoundOff();
    }

    OLED_View_ShowAlarm(alarm);
}

static void app_poll_net(void)
{
#if EN_ESP8266_ONENET
    ESP8266_OneNET_InitFsm_Poll();
    g_esp.wifi_ready = tls_inited ? 1u : 0u;
    g_esp.hb_ok = ESP8266_Online_Flag ? 1u : 0u;
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
#endif
}

int main(void)
{
    app_init();
    Security_Set_Mode(0);

    while(1)
    {
        KeyboardSM_Tick();
        LockManager_Tick();
        app_poll_sensors();
        app_poll_alarm_and_act();
        app_poll_net();
        Bare_CapturePoll();
        OLED_View_RefreshDashboard(&g_sensor, LockManager_GetState(), &g_esp);
        delay_ms(20);
    }
}