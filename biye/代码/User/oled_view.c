#include "oled_view.h"
#include "oled.h"
#include "board_config.h"
#include <stdio.h>

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
    OLED_ShowString(0, 16, "Input Password  ", 16);
    if(count >= 1u && count <= 6u)
    {
        OLED_ShowString((u8)(32u + (count - 1u) * 10u), 32, "*", 16);
    }
}

void OLED_View_OnPasswordCancel(void)
{
    OLED_ShowString(0, 16, "INPUT CANCELED  ", 16);
    OLED_ShowString(0, 32, "                ", 16);
    OLED_ShowString(0, 48, "                ", 16);
}

void OLED_View_OnPasswordTimeout(void)
{
    OLED_ShowString(0, 16, "INPUT TIMEOUT   ", 16);
    OLED_ShowString(0, 32, "Press * Retry   ", 16);
    OLED_ShowString(0, 48, "                ", 16);
}

void OLED_View_ShowAlarm(alarm_type_t alarm)
{
    switch(alarm)
    {
        case ALARM_PIR:
            OLED_ShowString(0, 48, "PIR ALARM       ", 16);
            break;
        case ALARM_GAS:
            OLED_ShowString(0, 48, "GAS ALARM       ", 16);
            break;
        case ALARM_BOTH:
            OLED_ShowString(0, 48, "ALARM BOTH      ", 16);
            break;
        default:
            OLED_ShowString(0, 48, "                ", 16);
            break;
    }
}

void OLED_View_ShowNetState(u8 wifi_ok, u8 hb_ok)
{
    if(wifi_ok)
        OLED_ShowString(0, 0, hb_ok ? "NET:OK HB:OK    " : "NET:OK HB:--    ", 16);
    else
        OLED_ShowString(0, 0, "NET:INIT        ", 16);
}

void OLED_View_RefreshDashboard(const sensor_state_t *sensor, const lock_state_t *lock, const esp_state_t *esp)
{
    char line[17];
    if(sensor == NULL || lock == NULL || esp == NULL)
        return;

    OLED_BatchBegin();
    OLED_View_ShowNetState(esp->wifi_ready, esp->hb_ok);
    if(lock->user_level == USER_ADMIN)
        OLED_ShowString(0, 16, "ADMIN MODE      ", 16);
    else if(lock->user_level == USER_NORMAL)
        OLED_ShowString(0, 16, "UNLOCK OK       ", 16);
    else
        OLED_ShowString(0, 16, "Input Password  ", 16);

    sprintf(line, "T:%02u H:%02u MQ:%u", sensor->temp, sensor->humi, sensor->mq2_adc);
    OLED_ShowString(0, 32, line, 16);
    OLED_ShowString(0, 48, lock->armed ? "ARMED           " : "DISARMED        ", 16);
    OLED_BatchEnd();
}
