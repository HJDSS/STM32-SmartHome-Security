#ifndef __ESP8266_TLS_H
#define __ESP8266_TLS_H

#include "sys.h"

u8 ESP8266_TLS_Init(void);
u8 ESP8266_AT_SendWait(char *cmd, char *ack, u16 timeout_ms);
void ESP8266_RESET(void);
void ESP8266_EN_GPIO_Init(void);

void OneNET_Parse_Cmd(void);
void OneNET_Publish_Data(u8 temp, u8 humi, u16 gas, u8 door, u8 arm, u8 alarm, u8 led);
void OneNET_Publish_Alarm(const char *alarm_type);

u8 OneNET_PropertySetPending(void);
const char *OneNET_PropertySetPayload(void);
const char *OneNET_PropertySetReplyId(void);
void OneNET_PropertySetClear(void);

u8 ESP8266_OneNET_Mqtt_Connect(void);

extern u8 tls_inited;
extern volatile u8 g_net_stage;
extern volatile u8 g_wifi_fail_reason;
extern volatile u8 g_wifi_substage;
extern volatile u8 g_wifi_err;
void ESP8266_CooperativeYield(void);
u8 ESP8266_OneNET_Full_Init(void);

void ESP8266_OneNET_InitFsm_Reset(void);
void ESP8266_OneNET_InitFsm_Poll(void);

u8 ESP8266_AT_Nb_Begin(char *cmd, char *ack, u16 timeout_ms);
u8 ESP8266_AT_Nb_Poll(void);
void ESP8266_WaitStr_Nb_Begin(const char *sub, u16 timeout_ms);
u8 ESP8266_WaitStr_Nb_Poll(void);
void ESP8266_WaitAny2_Nb_Begin(const char *a, const char *b, u16 timeout_ms);
u8 ESP8266_WaitAny2_Nb_Poll(void);

extern u8 Ctrl_Led;
extern u8 Ctrl_Door;
extern u8 Ctrl_Arm;
extern u8 ESP8266_Online_Flag;

#endif
