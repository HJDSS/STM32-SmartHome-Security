#include "linkage.h"
#include "board_config.h"
#include <stdio.h>
#include "usart1.h"
#include "string.h"
#include "syslog.h"
#include "gpio.h"
#include "oled.h"
#include "esp8266_tls.h"

extern u8 security_mode;
extern u8 security_alarm;

extern void Security_Set_Mode(u8 armed);

void Light_Relay_On(void)
{
#if !BOARD_PRINTF_SOFTUART_PC8
    GPIO_WriteBit(LIGHT_RELAY_PORT, LIGHT_RELAY_PIN, LIGHT_ON_LEVEL);
#endif
}

void Light_Relay_Off(void)
{
#if !BOARD_PRINTF_SOFTUART_PC8
    GPIO_WriteBit(LIGHT_RELAY_PORT, LIGHT_RELAY_PIN, LIGHT_OFF_LEVEL);
#endif
}

void Linkage_Init(void)
{
#if LINKAGE_EN
#if !BOARD_PRINTF_SOFTUART_PC8
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(LIGHT_RELAY_CLK, ENABLE);
    GPIO_InitStructure.GPIO_Pin = LIGHT_RELAY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LIGHT_RELAY_PORT, &GPIO_InitStructure);

    Light_Relay_Off();
#endif
#endif
}

void Linkage_MQTT_Report(char *msg)
{
#if DEBUG_PASSWORD_ONLY
    (void)msg;
    return;
#endif
#if LINKAGE_EN
    if(msg==NULL) return;
    uart1_SendStr(msg);
    uart1_SendStr("\r\n");
    /* 🟡10: 告警消息同时即时推送至 OneNET 云端 */
    if(strstr(msg, "ALARM:") != NULL)
    {
        OneNET_Publish_Alarm(msg);
    }
#else
    (void)msg;
#endif
}

void Linkage_OnUnlock(UNLOCK_SRC_T src)
{
    /* 门禁解锁：三声短促提示音（独立于 LINKAGE_EN，始终生效） */
    BEEP_StartPattern(BEEP_PATTERN_DOORBELL);
#if DEBUG_PASSWORD_ONLY
    (void)src;
    return;
#endif
#if (LINKAGE_EN && LINKAGE_UNLOCK_EN)
    (void)src;
#if LINKAGE_UNLOCK_LIGHT_ON
    Light_Relay_On();
    Linkage_MQTT_Report("EVT:UNLOCK_LIGHT_ON");
#endif
#else
    (void)src;
#endif
}

void Linkage_OnIntrusion(void)
{
#if (LINKAGE_EN && LINKAGE_INTRUSION_EN)
#if LINKAGE_INTRUSION_ALL_LIGHT_ON
    Light_Relay_On();
#endif
#if LINKAGE_INTRUSION_VOICE_UART_EN
    VOICE_UART_SEND_STR("VOICE: INTRUSION_WARNING!\r\n");
#endif
    Linkage_MQTT_Report("ALARM: INTRUSION");
#endif
}

void Linkage_OnGasState(u8 active)
{
#if DEBUG_PASSWORD_ONLY
    (void)active;
    return;
#endif
#if (LINKAGE_EN && LINKAGE_ENV_EN)
    static u8 s_have_last = 0u;
    static u8 s_last_active = 0xFFu;

    if(!s_have_last)
    {
        s_have_last = 1u;
        s_last_active = active;
        return;
    }
    if(active == s_last_active)
        return;
    s_last_active = active;

    if(active)
    {
        GAS_VALVE_RELAY = 0;
        OLED_ShowString(0u, 32u, "VALVE CLOSED    ", 16u);
        Linkage_MQTT_Report("ALARM:GAS_SMOKE");
        Linkage_MQTT_Report("EVT:VALVE_CLOSE");
        SysLog_Add(LOG_EVT_ALARM, "GAS_VALVE_CLOSE");
    }
    else
    {
        GAS_VALVE_RELAY = 1;
        OLED_ShowString(0u, 32u, "VALVE OPEN      ", 16u);
        Linkage_MQTT_Report("EVT:VALVE_OPEN_AUTO");
        SysLog_Add(LOG_EVT_CONFIG, "GAS_VALVE_REOPEN");
    }
#else
    (void)active;
#endif
}

void Linkage_OnMQ2_Alarm(void)
{
#if DEBUG_PASSWORD_ONLY
    return;
#endif
    Linkage_OnGasState(1u);
}

void Linkage_OnTempHumi_Alarm(u8 temp_alarm,u8 humi_alarm,u8 temp,u8 humi)
{
#if (LINKAGE_EN && LINKAGE_ENV_TEMP_HUMI_EN)
    if(temp_alarm)
    {
        Linkage_MQTT_Report("ALARM:TEMP");
    }
    if(humi_alarm)
    {
        Linkage_MQTT_Report("ALARM:HUMI");
    }
    (void)temp;
    (void)humi;
#else
    (void)temp_alarm;
    (void)humi_alarm;
    (void)temp;
    (void)humi;
#endif
}

void Linkage_MQTT_Cmd_Proc(char *cmd)
{
#if (LINKAGE_EN && LINKAGE_TIMER_EN)
    if(cmd==NULL) return;

    if(strstr(cmd,"LIGHT=ON")!=NULL) { Light_Relay_On(); Linkage_MQTT_Report("ACK:LIGHT_ON"); SysLog_Add(LOG_EVT_CONFIG,"LIGHT_ON"); }
    else if(strstr(cmd,"LIGHT=OFF")!=NULL) { Light_Relay_Off(); Linkage_MQTT_Report("ACK:LIGHT_OFF"); SysLog_Add(LOG_EVT_CONFIG,"LIGHT_OFF"); }
    else if(strstr(cmd,"ARM")!=NULL) { Security_Set_Mode(1); Linkage_MQTT_Report("ACK:ARM"); SysLog_Add(LOG_EVT_CONFIG,"ARM"); }
    else if(strstr(cmd,"DISARM")!=NULL) { Security_Set_Mode(0); Linkage_MQTT_Report("ACK:DISARM"); SysLog_Add(LOG_EVT_CONFIG,"DISARM"); }
#else
    (void)cmd;
#endif
}
