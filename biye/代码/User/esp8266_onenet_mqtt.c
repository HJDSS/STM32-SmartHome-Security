/*
 * OneNET MQTT（物模型）：ESP8266 AT+MQTT（ESP-AT 2.x）
 * 依赖：esp8266_tls.c 中 WiFi 已连通；Uart2_Buf 足够大（见 main.c Buf2_Max）
 */

#include "esp8266_onenet_mqtt.h"
#include "esp8266_tls.h"
#include "board_config.h"
#include "usart2.h"
#include "delay.h"
#include "string.h"
#include <stdio.h>
#include "stm32f10x_usart.h"

#define ESP_DELAY_MS(ms) delay_ms((u16)(ms))

extern char Uart2_Buf[];
extern void ESP8266_CooperativeYield(void);

/* [M1.11] MQTT 子阶段 + 失败代码，供 OLED_View_ShowNetState 显示，
 * 快速定位 "NET:ERR MQTT" 到底死在哪一步：
 *   g_mqtt_substage:
 *     0 = 刚进 FSM，发 AT+MQTTCLEAN=0
 *     1 = 等 MQTTCLEAN OK
 *     2 = 发 AT+MQTTUSERCFG=0,<scheme>,"cid","usr","pwd",0,0,""（短形式）
 *     3 = 短形式失败，走 LONG* 三条命令的长写
 *     5 = 发 AT+MQTTCONN=0,"broker",port,auto
 *     6 = 等 +MQTTCONNECTED: 或 +MQTTDISCONNECTED:
 *     7 = 订阅 post/reply 主题
 *     8 = 订阅 property/set 主题
 *     9 = 连通后常驻
 *   g_mqtt_err:
 *     0 = 无
 *     1 = MQTTCLEAN ERROR/超时（AT 固件可能不支持 MQTT 命令族）
 *     2 = 短形式 USERCFG 失败（走长形式还没最终失败）
 *     3 = 长形式 USERCFG 失败（AT 固件真的不支持 MQTT，或 token 太长被截）
 *     4 = MQTTCONN 返回 +MQTTDISCONNECTED（TCP 通了但 MQTT 鉴权被拒
 *           → 最常见原因：OneNET token 已过期 / sign 错 / 产品ID 错 / 设备名错）
 *     5 = MQTTCONN 返回 ERROR（AT 参数不合法或模组不支持该命令）
 *     6 = MQTTCONN 45s 超时（broker 域名解析失败 / 路由到 mqtts.heclouds.com 不通）
 *     7 = post/reply 订阅失败
 *     8 = property/set 订阅失败 */
volatile u8 g_mqtt_substage = 0u;
volatile u8 g_mqtt_err = 0u;

#if ONENET_MQTT_ENABLE

static void UART2_SendBuffer(const u8 *p, u16 len)
{
		u16 n = 0;
		while(len--)
		{
				while(USART_GetFlagStatus(BOARD_USART3_INSTANCE, USART_FLAG_TC) == RESET)
						;
				USART_SendData(BOARD_USART3_INSTANCE, (u16)*p++);
				if(++n >= 64u)
				{
						n = 0;
						ESP8266_CooperativeYield();
				}
		}
}

static u8 WaitSubstr(const char *sub, u16 timeout_ms)
{
		u16 t = 0;
		if(sub == NULL) return 0;
		while(t < timeout_ms)
		{
				if(strstr(Uart2_Buf, sub) != NULL) return 1;
				ESP8266_CooperativeYield();
				ESP_DELAY_MS(20);
				t += 20;
		}
		return 0;
}

static u8 WaitAny2(const char *a, const char *b, u16 timeout_ms)
{
		u16 t = 0;
		while(t < timeout_ms)
		{
				if(a && strstr(Uart2_Buf, a) != NULL) return 1;
				if(b && strstr(Uart2_Buf, b) != NULL) return 2;
				if(strstr(Uart2_Buf, "ERROR") != NULL) return 0;
				ESP8266_CooperativeYield();
				ESP_DELAY_MS(20);
				t += 20;
		}
		return 0;
}

static u8 MqttAT_SendLongBlob(const char *at_cmd_prefix, const u8 *data, u16 len, u16 first_ok_ms, u16 second_ok_ms)
{
		char cmd[48];
		if(at_cmd_prefix == NULL || data == NULL || len == 0) return 0;

		CLR_Buf2();
		sprintf(cmd, "%s0,%u", at_cmd_prefix, (unsigned)len);
		if(!ESP8266_AT_SendWait(cmd, "OK", first_ok_ms)) return 0;

		CLR_Buf2();
		UART2_SendBuffer(data, len);
		if(!WaitSubstr("OK", second_ok_ms)) return 0;
		if(strstr(Uart2_Buf, "ERROR") != NULL) return 0;
		return 1;
}

static u8 Mqtt_SubscribeTopic(const char *topic)
{
		char cmd[160];
		u8 r;

		if(topic == NULL || topic[0] == 0) return 0;

		sprintf(cmd, "AT+MQTTSUB=0,\"%s\",0", topic);
		CLR_Buf2();
		UART2_SendString(cmd);
		UART2_SendString("\r\n");
		r = WaitAny2("OK", "ALREADY SUBSCRIBE", 12000);
		if(r == 0) return 0;
		if(strstr(Uart2_Buf, "ERROR") != NULL) return 0;
		return 1;
}

u8 OneNET_AT_Mqtt_PublishRaw(const char *topic, const char *payload, u16 len)
{
		char cmd[160];
		if(topic == NULL || payload == NULL || len == 0) return 0;
		/* ESP8266 默认 MQTT 缓冲较小；过大容易 +MQTTPUB:FAIL */
		if(len > 512) return 0;

		CLR_Buf2();
		/* 与资料一致：QoS=0，retain=0 */
		sprintf(cmd, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0", topic, (int)len);
		UART2_SendString(cmd);
		UART2_SendString("\r\n");
		if(!WaitSubstr(">", 8000))
		{
				ESP8266_Online_Flag = 0;
				tls_inited = 0;
				ESP8266_OneNET_InitFsm_Reset();
				return 0;
		}
		UART2_SendBuffer((const u8 *)payload, len);

		{
				u8 r = WaitAny2("+MQTTPUB:OK", "+MQTTPUB:FAIL", 12000);
				if(r != 1)
				{
						ESP8266_Online_Flag = 0;
						tls_inited = 0;
						ESP8266_OneNET_InitFsm_Reset();
						return 0;
				}
		}
		if(strstr(Uart2_Buf, "ERROR") != NULL)
		{
				ESP8266_Online_Flag = 0;
				tls_inited = 0;
				ESP8266_OneNET_InitFsm_Reset();
				return 0;
		}
		return 1;
}

u8 ESP8266_OneNET_Mqtt_Connect(void)
{
		char topic[128];
		char cmd[420];
		const u8 *cid = (const u8 *)ONENET_MQTT_CLIENT_ID;
		const u8 *usr = (const u8 *)ONENET_MQTT_USERNAME;
		const u8 *pwd = (const u8 *)ONENET_MQTT_PASSWORD;
		u16 cid_len = (u16)strlen((const char *)cid);
		u16 usr_len = (u16)strlen((const char *)usr);
		u16 pwd_len = (u16)strlen((const char *)pwd);

		ESP8266_Online_Flag = 0;

		/* 断开旧会话，避免反复 init 时状态残留 */
		ESP8266_AT_SendWait("AT+MQTTCLEAN=0", "OK", 3000);

		/* 优先按资料一行配置（与串口手册一致）；若固件限制单条 AT 长度，则回退 LONG 路径 */
		sprintf(cmd,
				"AT+MQTTUSERCFG=0,%d,\"%s\",\"%s\",\"%s\",0,0,\"\"",
				(int)ONENET_MQTT_AT_SCHEME,
				ONENET_MQTT_CLIENT_ID,
				ONENET_MQTT_USERNAME,
				ONENET_MQTT_PASSWORD);
		if(!ESP8266_AT_SendWait(cmd, "OK", 8000))
		{
				sprintf(cmd, "AT+MQTTUSERCFG=0,%d,\"\",\"\",\"\",0,0,\"\"", (int)ONENET_MQTT_AT_SCHEME);
				if(!ESP8266_AT_SendWait(cmd, "OK", 5000)) return 0;
				if(!MqttAT_SendLongBlob("AT+MQTTLONGCLIENTID=", cid, cid_len, 3000, 8000)) return 0;
				if(!MqttAT_SendLongBlob("AT+MQTTLONGUSERNAME=", usr, usr_len, 3000, 8000)) return 0;
				if(!MqttAT_SendLongBlob("AT+MQTTLONGPASSWORD=", pwd, pwd_len, 3000, 8000)) return 0;
		}

		sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,%d",
				ONENET_MQTT_BROKER, (int)ONENET_MQTT_PORT, (int)ONENET_MQTT_AUTO_RECONNECT);
		CLR_Buf2();
		UART2_SendString(cmd);
		UART2_SendString("\r\n");
		{
				u8 r = WaitAny2("+MQTTCONNECTED:", "+MQTTDISCONNECTED:", 45000);
				if(r != 1)
				{
						if(strstr(Uart2_Buf, "ERROR") != NULL) return 0;
						return 0;
				}
		}

		/* 资料步骤：订阅上报应答 + 属性设置（下行） */
		sprintf(topic, "$sys/%s/%s/thing/property/post/reply", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
		if(!Mqtt_SubscribeTopic(topic)) return 0;

		sprintf(topic, "$sys/%s/%s/thing/property/set", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
		if(!Mqtt_SubscribeTopic(topic)) return 0;

		ESP8266_Online_Flag = 1;
		return 1;
}

/* ---------- 非阻塞 MQTT 建链（与 ESP8266_OneNET_Mqtt_Connect 等价） ---------- */
static u16 s_mqtt_fsm_st;
typedef struct
{
		u8 ac;
		u32 t0;
		u16 tm;
} msw_t;
static msw_t g_msw;
static char s_mqtt_topic[128];

static void msw_begin(u16 tm)
{
		g_msw.ac = 1;
		g_msw.t0 = Bare_GetTickMs();
		g_msw.tm = tm;
}

/* 0 等待 1 成功 2 ERROR 3 超时 */
static u8 msw_poll(void)
{
		if(!g_msw.ac)
				return 3;
		if(strstr(Uart2_Buf, "ERROR") != NULL)
		{
				g_msw.ac = 0;
				return 2;
		}
		if(strstr(Uart2_Buf, "OK") != NULL || strstr(Uart2_Buf, "ALREADY SUBSCRIBE") != NULL)
		{
				g_msw.ac = 0;
				return 1;
		}
		if((u32)(Bare_GetTickMs() - g_msw.t0) >= g_msw.tm)
		{
				g_msw.ac = 0;
				return 3;
		}
		ESP8266_CooperativeYield();
		return 0;
}

void ESP8266_OneNET_MqttFsm_Reset(void)
{
		s_mqtt_fsm_st = 0;
		g_msw.ac = 0;
}

u8 ESP8266_OneNET_MqttFsm_Poll(void)
{
		char cmd[420];
		u8 r;
		const u8 *cid = (const u8 *)ONENET_MQTT_CLIENT_ID;
		const u8 *usr = (const u8 *)ONENET_MQTT_USERNAME;
		const u8 *pwd = (const u8 *)ONENET_MQTT_PASSWORD;
		u16 cid_len = (u16)strlen((const char *)cid);
		u16 usr_len = (u16)strlen((const char *)usr);
		u16 pwd_len = (u16)strlen((const char *)pwd);

		switch(s_mqtt_fsm_st)
		{
		case 0:
				g_mqtt_substage = 0u;
				g_mqtt_err = 0u;
				ESP8266_Online_Flag = 0;
				ESP8266_AT_Nb_Begin("AT+MQTTCLEAN=0", "OK", 3000);
				s_mqtt_fsm_st = 1;
				g_mqtt_substage = 1u;
				return 0;
		case 1:
				r = ESP8266_AT_Nb_Poll();
				if(r == 0)
						return 0;
				if(r == 2)
				{
						g_mqtt_err = 1u;    /* MQTTCLEAN 失败 → AT 固件可能无 MQTT 命令族 */
						s_mqtt_fsm_st = 100;
						return 2;
				}
				sprintf(cmd,
						"AT+MQTTUSERCFG=0,%d,\"%s\",\"%s\",\"%s\",0,0,\"\"",
						(int)ONENET_MQTT_AT_SCHEME,
						ONENET_MQTT_CLIENT_ID,
						ONENET_MQTT_USERNAME,
						ONENET_MQTT_PASSWORD);
				ESP8266_AT_Nb_Begin(cmd, "OK", 8000);
				s_mqtt_fsm_st = 2;
				g_mqtt_substage = 2u;
				return 0;
		case 2:
				r = ESP8266_AT_Nb_Poll();
				if(r == 0)
						return 0;
				if(r == 1)
				{
						s_mqtt_fsm_st = 5;
						g_mqtt_substage = 5u;
						return 0;
				}
				g_mqtt_err = 2u;       /* 短形式失败，落到长形式兜底 */
				s_mqtt_fsm_st = 3;
				g_mqtt_substage = 3u;
				return 0;
		case 3:
				sprintf(cmd, "AT+MQTTUSERCFG=0,%d,\"\",\"\",\"\",0,0,\"\"", (int)ONENET_MQTT_AT_SCHEME);
				if(!ESP8266_AT_SendWait(cmd, "OK", 5000))
				{
						g_mqtt_err = 3u;
						s_mqtt_fsm_st = 100;
						return 2;
				}
				if(!MqttAT_SendLongBlob("AT+MQTTLONGCLIENTID=", cid, cid_len, 3000, 8000))
				{
						g_mqtt_err = 3u;
						s_mqtt_fsm_st = 100;
						return 2;
				}
				if(!MqttAT_SendLongBlob("AT+MQTTLONGUSERNAME=", usr, usr_len, 3000, 8000))
				{
						g_mqtt_err = 3u;
						s_mqtt_fsm_st = 100;
						return 2;
				}
				if(!MqttAT_SendLongBlob("AT+MQTTLONGPASSWORD=", pwd, pwd_len, 3000, 8000))
				{
						g_mqtt_err = 3u;
						s_mqtt_fsm_st = 100;
						return 2;
				}
				s_mqtt_fsm_st = 5;
				g_mqtt_substage = 5u;
				return 0;
		case 5:
				sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,%d",
						ONENET_MQTT_BROKER, (int)ONENET_MQTT_PORT, (int)ONENET_MQTT_AUTO_RECONNECT);
				CLR_Buf2();
				UART2_SendString(cmd);
				UART2_SendString("\r\n");
				ESP8266_WaitAny2_Nb_Begin("+MQTTCONNECTED:", "+MQTTDISCONNECTED:", 45000);
				s_mqtt_fsm_st = 6;
				g_mqtt_substage = 6u;
				return 0;
		case 6:
				r = ESP8266_WaitAny2_Nb_Poll();
				if(r == 0)
						return 0;
				if(r != 1)
				{
						/* [M1.11] 区分三种失败：ERROR=AT 参数/不支持；+MQTTDISCONNECTED:=鉴权被拒；
						 * 都不匹配=45s 超时（DNS/路由不通） */
						if(strstr(Uart2_Buf, "ERROR") != NULL)
								g_mqtt_err = 5u;
						else if(strstr(Uart2_Buf, "+MQTTDISCONNECTED") != NULL)
								g_mqtt_err = 4u;
						else
								g_mqtt_err = 6u;
						s_mqtt_fsm_st = 100;
						return 2;
				}
				sprintf(s_mqtt_topic, "$sys/%s/%s/thing/property/post/reply",
						ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
				sprintf(cmd, "AT+MQTTSUB=0,\"%s\",0", s_mqtt_topic);
				CLR_Buf2();
				UART2_SendString(cmd);
				UART2_SendString("\r\n");
				msw_begin(12000);
				s_mqtt_fsm_st = 7;
				g_mqtt_substage = 7u;
				return 0;
		case 7:
				r = msw_poll();
				if(r == 0)
						return 0;
				if(r != 1)
				{
						g_mqtt_err = 7u;
						s_mqtt_fsm_st = 100;
						return 2;
				}
				sprintf(s_mqtt_topic, "$sys/%s/%s/thing/property/set",
						ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
				sprintf(cmd, "AT+MQTTSUB=0,\"%s\",0", s_mqtt_topic);
				CLR_Buf2();
				UART2_SendString(cmd);
				UART2_SendString("\r\n");
				msw_begin(12000);
				s_mqtt_fsm_st = 8;
				g_mqtt_substage = 8u;
				return 0;
		case 8:
				r = msw_poll();
				if(r == 0)
						return 0;
				if(r != 1)
				{
						g_mqtt_err = 8u;
						s_mqtt_fsm_st = 100;
						return 2;
				}
				ESP8266_Online_Flag = 1;
				s_mqtt_fsm_st = 9;
				g_mqtt_substage = 9u;
				g_mqtt_err = 0u;
				return 1;
		case 9:
				return 1;
		case 100:
				return 2;
		default:
				s_mqtt_fsm_st = 100;
				return 2;
		}
}

#else /* !ONENET_MQTT_ENABLE */

void ESP8266_OneNET_MqttFsm_Reset(void)
{
}

u8 ESP8266_OneNET_MqttFsm_Poll(void)
{
		return 1;
}

u8 OneNET_AT_Mqtt_PublishRaw(const char *topic, const char *payload, u16 len)
{
		(void)topic; (void)payload; (void)len;
		return 0;
}

u8 ESP8266_OneNET_Mqtt_Connect(void)
{
		return 1;
}

#endif
