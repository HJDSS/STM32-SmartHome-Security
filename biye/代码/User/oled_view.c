#include "oled_view.h"
#include "oled.h"
#include "board_config.h"
#include "delay.h"
#include "esp8266_tls.h"
#include "esp8266_onenet_mqtt.h"
#include <stdio.h>

static void oled_show_safe_line(u8 y, const char *msg)
{
    char line[17];
    u8 i;

    line[0] = ' ';
    for(i = 0u; i < 14u && msg != NULL && msg[i] != '\0'; i++)
        line[i + 1u] = msg[i];
    for(; i < 15u; i++)
        line[i + 1u] = ' ';
    line[16] = '\0';
    OLED_ShowString(0, y, line, 16);
}

void OLED_View_Init(void)
{
    OLED_Init();
    OLED_Clear();
}

void OLED_View_ShowBootSelfTest(void)
{
#if OLED_POWERON_SELFTEST_ENABLE
    OLED_SelfTest_PanelAndColumn();
    delay_ms((u16)OLED_POWERON_SELFTEST_MS);
#endif
}

void OLED_View_OnPasswordDigit(u8 count, int key)
{
    (void)key;
    oled_show_safe_line(16, "INPUT PASS");
    if(count >= 1u && count <= 6u)
    {
        OLED_ShowString((u8)(32u + (count - 1u) * 10u), 32, "*", 16);
    }
}

void OLED_View_OnPasswordCancel(void)
{
    oled_show_safe_line(16, "INPUT CANCEL");
    oled_show_safe_line(32, "");
    oled_show_safe_line(48, "");
}

void OLED_View_OnPasswordTimeout(void)
{
    oled_show_safe_line(16, "INPUT TIMEOUT");
    oled_show_safe_line(32, "PRESS * RETRY");
    oled_show_safe_line(48, "");
}

void OLED_View_ShowAlarm(alarm_type_t alarm)
{
    switch(alarm)
    {
        case ALARM_PIR:
            oled_show_safe_line(48, "PIR ALARM");
            break;
        case ALARM_GAS:
            oled_show_safe_line(48, "GAS ALARM");
            break;
        case ALARM_BOTH:
            oled_show_safe_line(48, "ALARM BOTH");
            break;
        default:
            oled_show_safe_line(48, "");
            break;
    }
}

void OLED_View_ToastRowAB(const char *line16, u32 duration_ms)
{
    char buf[17];
    u8 i;

    (void)duration_ms;
    if(line16 == NULL)
    {
        oled_show_safe_line(48, "");
        return;
    }
    for(i = 0u; i < 16u && line16[i] != 0; i++)
        buf[i] = line16[i];
    for(; i < 16u; i++)
        buf[i] = ' ';
    buf[16] = '\0';
    oled_show_safe_line(48, buf);
}

static const char *net_stage_name(u8 s)
{
    switch(s)
    {
        case 0:  return "IDLE";
        case 1:  return "AT  ";
        case 2:  return "WIFI";
        case 3:  return "MQTT";
        case 4:  return "OK  ";
        default: return "????";
    }
}

void OLED_View_ShowNetState(u8 wifi_ok, u8 hb_ok)
{
    char line[17];
    u8 sub;
    u8 stage;
    u8 err;
    u8 fail;

    if(wifi_ok)
    {
        oled_show_safe_line(0, hb_ok ? "NET OK HB OK" : "NET OK HB --");
        return;
    }

    fail = g_wifi_fail_reason;
    if(fail != 0u)
    {
        switch(fail)
        {
            case 1u:
                oled_show_safe_line(0, "NET ERR AT");
                break;
            case 2u:
                err = g_wifi_err;
                if(err > 9u) err = 9u;
                sprintf(line, "NET ERR WF e%u", (unsigned)err);
                line[16] = '\0';
                oled_show_safe_line(0, line);
                break;
            case 3u:
                {
                    u8 msub = g_mqtt_substage;
                    u8 merr = g_mqtt_err;
                    if(msub > 9u) msub = 9u;
                    if(merr > 9u) merr = 9u;
                    sprintf(line, "MQTT ERR %u/%u", (unsigned)msub, (unsigned)merr);
                    line[16] = '\0';
                    oled_show_safe_line(0, line);
                }
                break;
            default:
                oled_show_safe_line(0, "NET ERR ?");
                break;
        }
        return;
    }

    stage = g_net_stage;
    sub   = g_wifi_substage;
    err   = g_wifi_err;
    if(sub > 99u) sub = 99u;
    if(err > 9u)  err = 9u;

    sprintf(line, "NET %s %02u/%u", net_stage_name(stage), (unsigned)sub, (unsigned)err);
    line[16] = '\0';
    oled_show_safe_line(0, line);
}

void OLED_View_RefreshDashboard(const sensor_state_t *sensor, const lock_state_t *lock, const esp_state_t *esp)
{
    char line[17];
    const char *arm_txt;
    if(sensor == NULL || lock == NULL || esp == NULL)
        return;

    OLED_BatchBegin();
    OLED_View_ShowNetState(esp->wifi_ready, esp->hb_ok);
    if(lock->user_level == USER_ADMIN)
        oled_show_safe_line(16, "ADMIN MODE");
    else if(lock->user_level == USER_NORMAL)
        oled_show_safe_line(16, "UNLOCK OK");
    else
        oled_show_safe_line(16, "INPUT PASS");

    sprintf(line, "T%02u H%02u G%04u", sensor->temp, sensor->humi, (unsigned)sensor->mq2_adc);
    line[16] = '\0';
    oled_show_safe_line(32, line);

    arm_txt = lock->armed ? "ON" : "OFF";
    sprintf(line, "ARM %s %s", arm_txt, lock->relay_on ? "OPEN" : "LOCK");
    line[16] = '\0';
    oled_show_safe_line(48, line);
    OLED_BatchEnd();
}
