#ifndef __OLED_VIEW_H
#define __OLED_VIEW_H

#include "app_types.h"

void OLED_View_Init(void);
void OLED_View_ShowBootSelfTest(void);
void OLED_View_RefreshDashboard(const sensor_state_t *sensor, const lock_state_t *lock, const esp_state_t *esp);
void OLED_View_OnPasswordDigit(u8 count, int key);
void OLED_View_OnPasswordCancel(void);
void OLED_View_OnPasswordTimeout(void);
void OLED_View_ShowAlarm(alarm_type_t alarm);
void OLED_View_ShowNetState(u8 wifi_ok, u8 hb_ok);

#endif
