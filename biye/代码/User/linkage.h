#ifndef __LINKAGE_H
#define __LINKAGE_H

#include "sys.h"
#include "board_config.h"

#define LINKAGE_EN                       1

#define LINKAGE_UNLOCK_EN                1
#define LINKAGE_UNLOCK_LIGHT_ON          1

#define LINKAGE_INTRUSION_EN             1
#define LINKAGE_INTRUSION_ALL_LIGHT_ON   1
#define LINKAGE_INTRUSION_VOICE_UART_EN  1

#define LINKAGE_ENV_EN                   1
#define LINKAGE_ENV_TEMP_HUMI_EN         1

#define LINKAGE_TIMER_EN                 1

#define LIGHT_RELAY_PORT                 BOARD_LIGHT_RELAY_PORT
#define LIGHT_RELAY_PIN                  BOARD_LIGHT_RELAY_PIN
#define LIGHT_RELAY_CLK                  BOARD_LIGHT_RELAY_CLK

#define LIGHT_ON_LEVEL                   Bit_RESET
#define LIGHT_OFF_LEVEL                  Bit_SET

#define VOICE_UART_SEND_STR(s)           uart1_SendStr((char*)(s))

typedef enum
{
    UNLOCK_SRC_PWD = 0,
    UNLOCK_SRC_FINGER,
    UNLOCK_SRC_REMOTE
}UNLOCK_SRC_T;

void Linkage_Init(void);
void Linkage_OnUnlock(UNLOCK_SRC_T src);
void Linkage_OnIntrusion(void);
void Linkage_OnMQ2_Alarm(void);
void Linkage_OnGasState(u8 active);
void Linkage_OnTempHumi_Alarm(u8 temp_alarm,u8 humi_alarm,u8 temp,u8 humi);
void Linkage_MQTT_Report(char *msg);
void Linkage_MQTT_Cmd_Proc(char *cmd);

void Light_Relay_On(void);
void Light_Relay_Off(void);

#endif
