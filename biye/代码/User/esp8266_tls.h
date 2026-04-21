#ifndef __ESP8266_TLS_H
#define __ESP8266_TLS_H

#include "sys.h"

/* WiFi / 模块基础 AT（MQTT 仅经 AT+MQTT，见 esp8266_onenet_mqtt.c） */
u8 ESP8266_TLS_Init(void);
u8 ESP8266_AT_SendWait(char *cmd, char *ack, u16 timeout_ms);
void ESP8266_RESET(void);

void OneNET_Parse_Cmd(void);
void OneNET_Publish_Data(u8 temp, u8 humi, u16 gas, u8 door, u8 arm, u8 alarm, u8 led);

/* AT+MQTT 建链（实现在 esp8266_onenet_mqtt.c） */
u8 ESP8266_OneNET_Mqtt_Connect(void);

/* WiFi+MQTT 全成功后才为 1；失败保持 0，由 ESP8266_OneNET_Full_Init 维护 */
extern u8 tls_inited;
/* 联网阶段：0=IDLE 1=AT 2=WIFI 3=MQTT 4=OK（便于 OLED 快速定位失败点） */
extern volatile u8 g_net_stage;
/* 无串口时用于 OLED 快速定位失败原因：0=无 1=AT握手 2=WiFi连接 3=MQTT连接 */
extern volatile u8 g_wifi_fail_reason;
/* WiFi 子阶段（仅用于OLED定位卡点）：
 * 0=IDLE 1=AT 20=CWMODE 21=CWJAP 22=WAIT_IP 23=OK
 */
extern volatile u8 g_wifi_substage;
/* WiFi 细分错误码（仅用于 OLED 定位）：
 * 0=无 1=CWLAP找不到SSID 2=CWLAP信道不支持 3=CWJAP超时/失败 4=CWMODE失败
 */
extern volatile u8 g_wifi_err;
/* 裸机模式下在 AT 长等待中调用，避免阻塞主循环导致键盘“假死”（实现见 main.c） */
void ESP8266_CooperativeYield(void);
u8 ESP8266_OneNET_Full_Init(void);

/* 裸机非阻塞：OneNET/ESP 初始化状态机（每圈主循环调用 Poll） */
void ESP8266_OneNET_InitFsm_Reset(void);
void ESP8266_OneNET_InitFsm_Poll(void);

/* AT 非阻塞：Begin 发一条 AT，之后反复 Poll 直到 OK 或超时 */
u8 ESP8266_AT_Nb_Begin(char *cmd, char *ack, u16 timeout_ms);
u8 ESP8266_AT_Nb_Poll(void);
void ESP8266_WaitStr_Nb_Begin(const char *sub, u16 timeout_ms);
u8 ESP8266_WaitStr_Nb_Poll(void);
void ESP8266_WaitAny2_Nb_Begin(const char *a, const char *b, u16 timeout_ms);
/* 0 等待 1 匹配 a 2 匹配 b 3 超时或 ERROR */
u8 ESP8266_WaitAny2_Nb_Poll(void);

extern u8 Ctrl_Led;
extern u8 Ctrl_Door;
extern u8 Ctrl_Arm;
extern u8 ESP8266_Online_Flag;

#endif
