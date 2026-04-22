#include "esp8266_tls.h"
#include "esp8266_onenet_mqtt.h"
#include "board_config.h"
#include "usart2.h"
#include "delay.h"
#include "string.h"
#include <stdio.h>
#include "log.h"
#include "app_params.h"

#define ESP_DELAY_MS(ms) delay_ms((u16)(ms))

/* 映射到工程已有 uart1_SendStr（不增加头文件） */
extern void uart1_SendStr(char *SendBuf);
#define UART1_SendStr uart1_SendStr

/* ================= WiFi STA（与 OneNET 产品信息见 esp8266_onenet_mqtt.h） ================= */
#define WIFI_SSID    BOARD_WIFI_SSID
#define WIFI_PASS    BOARD_WIFI_PASS
/* CWJAP 超时过会长占 CPU 时间片，低优先级 OLED 易“假死”；可在包含本文件前 #define 覆盖 */
#ifndef WIFI_CWJAP_TIMEOUT_MS
#define WIFI_CWJAP_TIMEOUT_MS  20000u
#endif
#ifndef WIFI_CWJAP_RETRY_COUNT
#define WIFI_CWJAP_RETRY_COUNT  2u
#endif
/* ESP-01 上电后需要一点启动时间；否则 AT/CWJAP 早发会表现为“连不上/无响应” */
#ifndef ESP_AT_BOOT_DELAY_MS
#define ESP_AT_BOOT_DELAY_MS   1200u
#endif
#ifndef ESP_AT_HANDSHAKE_RETRIES
#define ESP_AT_HANDSHAKE_RETRIES  5u
#endif
/* 预配置 WiFi：上电后 DHCP 可能尚未下发 STAIP，需多次 CIFSR；每次间隔内让出 CPU */
#ifndef WIFI_PRECONF_CIFSR_RETRIES
#define WIFI_PRECONF_CIFSR_RETRIES  50u
#endif
#ifndef WIFI_PRECONF_CIFSR_GAP_MS
#define WIFI_PRECONF_CIFSR_GAP_MS   400u
#endif
/* CIFSR 应答读完后多等一会，避免 ISR 尚未拼完最后一行就判失败 */
#ifndef WIFI_PRECONF_CIFSR_POST_OK_MS
#define WIFI_PRECONF_CIFSR_POST_OK_MS  60u
#endif

/* CIFSR 回复中是否已出现 STA 侧 IP（不同 AT 版本字段名略有差异） */
static u8 cifsr_buf_has_sta_ip(const char *buf)
{
		if(buf == NULL || buf[0] == 0) return 0u;
		if(strstr(buf, "STAIP") != NULL) return 1u;
		if(strstr(buf, "192.") != NULL) return 1u;
		if(strstr(buf, "10.") != NULL) return 1u;
		if(strstr(buf, "172.16.") != NULL || strstr(buf, "172.17.") != NULL
				|| strstr(buf, "172.18.") != NULL || strstr(buf, "172.19.") != NULL
				|| strstr(buf, "172.2") != NULL || strstr(buf, "172.3") != NULL)
				return 1u;
		return 0u;
}

/* 平台下行控制（供主程序 FreeRTOS 任务读取，等价 uint8_t） */
u8 Ctrl_Led;
u8 Ctrl_Door;
u8 Ctrl_Arm;
u8 ESP8266_Online_Flag;

/* 仅 WiFi+AT+MQTT 鉴权全成功后置 1，见 ESP8266_OneNET_Full_Init */
u8 tls_inited = 0;
volatile u8 g_net_stage = 0;
volatile u8 g_wifi_fail_reason = 0;
volatile u8 g_wifi_substage = 0;
volatile u8 g_wifi_err = 0;
volatile u32 g_net_reconnect_count = 0;
volatile u32 g_net_offline_count = 0;

/* 前向声明：供 CWLAP 辅助函数使用 */
static u8 WaitSubstr(const char *sub, u16 timeout_ms);
static void ESP8266_Debug_DumpBootInfo(void);
static void Net_LogSimple(const char *msg);
static void Net_MarkOffline(const char *reason);

static void Net_LogSimple(const char *msg)
{
		if(msg == NULL) return;
		/* 兼容既有输出，但统一格式给测试复现用 */
		LOG_NET("%s", msg);
}

static void Net_MarkOffline(const char *reason)
{
		if(ESP8266_Online_Flag != 0u)
		{
				g_net_offline_count++;
				LOG_NET("offline");
		}
		ESP8266_Online_Flag = 0u;
		tls_inited = 0u;
		if(reason != NULL)
		{
				LOG_NET("reason: %s", reason);
		}
}

/* 2.4G 常用信道上限：手机热点若跑到 12/13 往往连不上 */
#ifndef WIFI_MAX_CHANNEL
#define WIFI_MAX_CHANNEL  11u
#endif
/* 1=连接前先 CWLAP 扫描；0=跳过扫描直接 CWJAP（部分老 AT 固件更稳定） */
#ifndef WIFI_USE_CWLAP_PRECHECK
#define WIFI_USE_CWLAP_PRECHECK  0u
#endif

/* CWLAP 只扫指定 SSID，并解析信道（若可解析） */
#if WIFI_USE_CWLAP_PRECHECK
static u8 esp_cwlap_check_ssid(const char *ssid, u8 *ch_out)
{
		char cmd[96];
		char *p;
		int ch = 0;

		if(ch_out) *ch_out = 0u;
		if(ssid == NULL || ssid[0] == 0) return 0u;

		/* 先试带 SSID 过滤；部分 AT 固件不支持该语法，失败则回退到全量扫描 */
		CLR_Buf2();
		sprintf(cmd, "AT+CWLAP=\"%s\"", ssid);
		UART2_SendString(cmd);
		UART2_SendString("\r\n");
		if(!WaitSubstr("OK", 6000))
		{
				CLR_Buf2();
				UART2_SendString("AT+CWLAP\r\n");
				if(!WaitSubstr("OK", 7000))
						return 0u;
		}
		if(strstr(Uart2_Buf, "+CWLAP:") == NULL)
				return 0u;
		/* 必须包含目标 SSID，避免扫到别的 AP 误判为“找到” */
		{
				char key[48];
				sprintf(key, "\"%s\"", ssid);
				if(strstr(Uart2_Buf, key) == NULL)
						return 0u;
		}

		/* +CWLAP:(<ecn>,\"ssid\",<rssi>,\"mac\",<channel>...) 尝试取 channel */
		{
				char key2[48];
				sprintf(key2, "\"%s\"", ssid);
				p = strstr(Uart2_Buf, key2);
				if(p != NULL)
				{
						/* 回退到该 SSID 所在的 +CWLAP 行开头附近 */
						while((p > Uart2_Buf) && (*(p-1) != '\n')) p--;
				}
				else
				{
						p = strstr(Uart2_Buf, "+CWLAP:(");
				}
		}
		if(p != NULL)
		{
				char *last_comma = NULL;
				char *q = p;
				while(*q)
				{
						if(*q == ',') last_comma = q;
						q++;
				}
				if(last_comma && sscanf(last_comma + 1, "%d", &ch) == 1)
				{
						if(ch_out && ch > 0 && ch < 100) *ch_out = (u8)ch;
				}
		}
		return 1u;
}
#endif

/* ESP8266 EN/CH_PD 硬件复位：按 BOARD_ESP8266_EN_* 引脚拉低 100ms 后拉高 300ms */
void ESP8266_RESET(void)
{
#if BOARD_WIFI_USE_NODEMCU
		/* EN/CH_PD 不由 MCU 控制时，跳过硬复位，避免误拉低导致模块掉电 */
		UART1_SendStr("[OneNET] ESP8266_RESET skipped (EN not MCU controlled)\r\n");
		return;
#else
		u8 i;
		GPIO_WriteBit(BOARD_ESP8266_EN_PORT, BOARD_ESP8266_EN_PIN, Bit_RESET);
		for(i = 0; i < 5; i++) { ESP8266_CooperativeYield(); ESP_DELAY_MS(20); }
		GPIO_WriteBit(BOARD_ESP8266_EN_PORT, BOARD_ESP8266_EN_PIN, Bit_SET);
		for(i = 0; i < 15; i++) { ESP8266_CooperativeYield(); ESP_DELAY_MS(20); }
		UART1_SendStr("[OneNET] ESP8266硬件复位完成\r\n");
#endif
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

static void ESP8266_Debug_DumpBootInfo(void)
{
#if BOARD_WIFI_DEBUG_BOOTINFO
		CLR_Buf2();
		UART2_SendString("AT+GMR\r\n");
		if(WaitSubstr("OK", 1500))
		{
				UART1_SendStr("[ESP01S] AT+GMR:\r\n");
				UART1_SendStr(Uart2_Buf);
				UART1_SendStr("\r\n");
		}

		CLR_Buf2();
		UART2_SendString("AT+CWMODE?\r\n");
		if(WaitSubstr("OK", 1200))
		{
				UART1_SendStr("[ESP01S] AT+CWMODE?:\r\n");
				UART1_SendStr(Uart2_Buf);
				UART1_SendStr("\r\n");
		}

		CLR_Buf2();
		UART2_SendString("AT+CWSTATE?\r\n");
		if(WaitSubstr("OK", 1200))
		{
				UART1_SendStr("[ESP01S] AT+CWSTATE?:\r\n");
				UART1_SendStr(Uart2_Buf);
				UART1_SendStr("\r\n");
		}
#endif
}

/* 通用等待：OK 成功；遇到 ERROR/FAIL/busy 立即失败，避免“看起来卡住” */
static u8 WaitOkOrError(u16 timeout_ms)
{
		u16 t = 0;
		while(t < timeout_ms)
		{
				if(strstr(Uart2_Buf, "OK") != NULL) return 1u;
				if(strstr(Uart2_Buf, "ERROR") != NULL) return 0u;
				if(strstr(Uart2_Buf, "FAIL") != NULL) return 0u;
				if(strstr(Uart2_Buf, "busy") != NULL) return 0u;
				ESP8266_CooperativeYield();
				ESP_DELAY_MS(20);
				t += 20;
		}
		return 0u;
}

/* CWJAP 专用等待：遇到 FAIL/ERROR/NO AP 立即失败，不必等满超时 */
static u8 WaitCwjapOkOrFail(u16 timeout_ms)
{
		u16 t = 0;
		while(t < timeout_ms)
		{
				if(strstr(Uart2_Buf, "OK") != NULL) return 1u;
				if(strstr(Uart2_Buf, "FAIL") != NULL) return 0u;
				if(strstr(Uart2_Buf, "ERROR") != NULL) return 0u;
				if(strstr(Uart2_Buf, "No AP") != NULL) return 0u;
				if(strstr(Uart2_Buf, "WIFI DISCONNECT") != NULL) return 0u;
				ESP8266_CooperativeYield();
				ESP_DELAY_MS(20);
				t += 20;
		}
		return 0u;
}

/* 已关联路由器（预配置模式下 CIFSR 偶发无 STAIP 字段，但 STA 已在线） */
#if BOARD_WIFI_PRECONFIGURED
static u8 esp_cwjap_is_associated(void)
{
		CLR_Buf2();
		UART2_SendString("AT+CWJAP?\r\n");
		if(!WaitSubstr("OK", 4000))
				return 0u;
		if(strstr(Uart2_Buf, "No AP") != NULL)
				return 0u;
		if(strstr(Uart2_Buf, "CWJAP:FAIL") != NULL)
				return 0u;
		if(strstr(Uart2_Buf, "+CWJAP:\"") != NULL)
				return 1u;
		if(strstr(Uart2_Buf, "+CWJAP_CUR:\"") != NULL)
				return 1u;
		return 0u;
}

/* 部分 AT 版本用 CIPSTA_CUR 查询当前 STA IP */
static u8 try_read_sta_ip_cipsta_cur(void)
{
		CLR_Buf2();
		UART2_SendString("AT+CIPSTA_CUR?\r\n");
		if(!WaitSubstr("OK", 4000))
				return 0u;
		return cifsr_buf_has_sta_ip(Uart2_Buf);
}
#endif

/** STA 模式并连接 AP（CWJAP 超时 WIFI_CWJAP_TIMEOUT_MS，默认 10s） */
static u8 ESP8266_WiFi_Join(void)
{
#if BOARD_WIFI_PRECONFIGURED
		/* 预配置模式：仅确认 STA 模式与已获 IP；DHCP 可能晚几秒才出现 STAIP */
		{
				u8 n;
				g_wifi_substage = 20u;
				if(!ESP8266_AT_SendWait("AT+CWMODE=1","OK",3000)) return 0;
				g_wifi_substage = 22u;
				for(n = 0u; n < (u8)WIFI_PRECONF_CIFSR_RETRIES; n++)
				{
						CLR_Buf2();
						UART2_SendString("AT+CIFSR\r\n");
						if(WaitSubstr("OK", 4000))
						{
								ESP_DELAY_MS((u16)WIFI_PRECONF_CIFSR_POST_OK_MS);
								/* 必须像手册一样含 +CIFSR行，避免误匹配到杂散 OK */
								if(strstr(Uart2_Buf, "+CIFSR") != NULL && cifsr_buf_has_sta_ip(Uart2_Buf))
										return 1;
						}
						if(try_read_sta_ip_cipsta_cur())
								return 1;
						ESP8266_CooperativeYield();
						ESP_DELAY_MS((u16)WIFI_PRECONF_CIFSR_GAP_MS);
				}
				if(esp_cwjap_is_associated())
				{
						UART1_SendStr("[WiFi] preconf: CWJAP linked, proceed (CIFSR may omit STAIP)\r\n");
						g_wifi_substage = 23u;
						return 1;
				}
				UART1_SendStr("[WiFi] preconf: join fail, last Uart2_Buf:\r\n");
				UART1_SendStr(Uart2_Buf);
				UART1_SendStr("\r\n");
		}
		return 0;
#else
		char cmd[128];
		u8 n;
		g_wifi_substage = 20u;
		/* 先查当前模式；已是 STA 则跳过设置，减少老 AT 固件卡在 CWMODE 的概率 */
		CLR_Buf2();
		UART2_SendString("AT+CWMODE?\r\n");
		if(!(WaitSubstr("OK", 2500) && (strstr(Uart2_Buf, "+CWMODE:1") != NULL || strstr(Uart2_Buf, "+CWMODE_CUR:1") != NULL)))
		{
				/* 兼容不同 ESP-AT：优先 CWMODE，其次 CWMODE_CUR；ERROR/busy 立即失败 */
				CLR_Buf2();
				UART2_SendString("AT+CWMODE=1\r\n");
				if(!WaitOkOrError(3000))
				{
						CLR_Buf2();
						UART2_SendString("AT+CWMODE_CUR=1\r\n");
						if(!WaitOkOrError(3000))
						{
								g_wifi_err = 4u;
								UART1_SendStr("[WiFi] CWMODE set fail, rsp:\r\n");
								UART1_SendStr(Uart2_Buf);
								UART1_SendStr("\r\n");
								return 0;
						}
				}
		}
		/* 预扫热点（可选）；某些 ESP-AT 固件对 CWLAP 兼容差，默认关闭预扫直接连 */
#if WIFI_USE_CWLAP_PRECHECK
		{
				u8 ch = 0u;
				g_wifi_substage = 210u; /* CWLAP */
				if(!esp_cwlap_check_ssid(WIFI_SSID, &ch))
				{
						g_wifi_err = 1u;
						return 0;
				}
				if(ch != 0u && ch > (u8)WIFI_MAX_CHANNEL)
				{
						g_wifi_err = 2u;
						return 0;
				}
		}
#endif
		g_wifi_substage = 21u;
		/* 先断开历史连接，避免旧状态干扰新热点连接 */
		ESP8266_AT_SendWait("AT+CWQAP", "OK", 2000);
		for(n = 0u; n < (u8)WIFI_CWJAP_RETRY_COUNT; n++)
		{
				/* 兼容不同 AT 版本：先试 CWJAP，再试 CWJAP_CUR */
				CLR_Buf2();
				sprintf(cmd,"AT+CWJAP=\"%s\",\"%s\"",WIFI_SSID,WIFI_PASS);
				UART2_SendString(cmd);
				UART2_SendString("\r\n");
				if(WaitCwjapOkOrFail((u16)WIFI_CWJAP_TIMEOUT_MS))
				{
						g_wifi_substage = 22u;
						return 1;
				}

				CLR_Buf2();
				sprintf(cmd,"AT+CWJAP_CUR=\"%s\",\"%s\"",WIFI_SSID,WIFI_PASS);
				UART2_SendString(cmd);
				UART2_SendString("\r\n");
				if(WaitCwjapOkOrFail((u16)WIFI_CWJAP_TIMEOUT_MS))
				{
						g_wifi_substage = 22u;
						return 1;
				}
				ESP8266_CooperativeYield();
				ESP_DELAY_MS(200);
		}
		g_wifi_err = 3u;
		UART1_SendStr("[WiFi] CWJAP fail, rsp:\r\n");
		UART1_SendStr(Uart2_Buf);
		UART1_SendStr("\r\n");
		return 0;
#endif
}

u8 ESP8266_AT_SendWait(char *cmd,char *ack,u16 timeout_ms)
{
		u16 t=0;
		if(cmd==NULL) return 0;
		CLR_Buf2();
		UART2_SendString(cmd);
		UART2_SendString("\r\n");

		while(t < timeout_ms)
		{
				if(ack==NULL) return 1;
				if(strstr(Uart2_Buf,ack)!=NULL) return 1;
				ESP8266_CooperativeYield();
				ESP_DELAY_MS(20);
				t += 20;
		}
		return 0;
}

/* ---------- 非阻塞 AT / 等待（供 OneNET 初始化状态机，每圈 Poll） ---------- */
typedef struct
{
	u8 active;
	u32 t0;
	u16 tmo_ms;
	char *ack;
} at_nb_t;
static at_nb_t g_at_nb;

typedef struct
{
	u8 active;
	u32 t0;
	u16 tmo_ms;
	const char *sub;
} wait_nb_t;
static wait_nb_t g_wait_nb;

typedef struct
{
	u8 active;
	u32 t0;
	u16 tmo_ms;
	const char *a;
	const char *b;
} wait2_nb_t;
static wait2_nb_t g_wait2_nb;

u8 ESP8266_AT_Nb_Begin(char *cmd, char *ack, u16 timeout_ms)
{
	if(cmd == NULL)
		return 0;
	CLR_Buf2();
	UART2_SendString(cmd);
	UART2_SendString("\r\n");
	g_at_nb.t0 = Bare_GetTickMs();
	g_at_nb.tmo_ms = timeout_ms;
	g_at_nb.ack = ack;
	g_at_nb.active = 1;
	return 1;
}

u8 ESP8266_AT_Nb_Poll(void)
{
	if(!g_at_nb.active)
		return 2;
	if(g_at_nb.ack != NULL && strstr(Uart2_Buf, g_at_nb.ack) != NULL)
	{
		g_at_nb.active = 0;
		return 1;
	}
	if((u32)(Bare_GetTickMs() - g_at_nb.t0) >= g_at_nb.tmo_ms)
	{
		g_at_nb.active = 0;
		return 2;
	}
	ESP8266_CooperativeYield();
	return 0;
}

void ESP8266_WaitStr_Nb_Begin(const char *sub, u16 timeout_ms)
{
	g_wait_nb.active = 1;
	g_wait_nb.t0 = Bare_GetTickMs();
	g_wait_nb.tmo_ms = timeout_ms;
	g_wait_nb.sub = sub;
}

u8 ESP8266_WaitStr_Nb_Poll(void)
{
	if(!g_wait_nb.active)
		return 2;
	if(g_wait_nb.sub != NULL && strstr(Uart2_Buf, g_wait_nb.sub) != NULL)
	{
		g_wait_nb.active = 0;
		return 1;
	}
	if((u32)(Bare_GetTickMs() - g_wait_nb.t0) >= g_wait_nb.tmo_ms)
	{
		g_wait_nb.active = 0;
		return 2;
	}
	ESP8266_CooperativeYield();
	return 0;
}

void ESP8266_WaitAny2_Nb_Begin(const char *a, const char *b, u16 timeout_ms)
{
	g_wait2_nb.active = 1;
	g_wait2_nb.t0 = Bare_GetTickMs();
	g_wait2_nb.tmo_ms = timeout_ms;
	g_wait2_nb.a = a;
	g_wait2_nb.b = b;
}

u8 ESP8266_WaitAny2_Nb_Poll(void)
{
	if(!g_wait2_nb.active)
		return 3;
	if(g_wait2_nb.a != NULL && strstr(Uart2_Buf, g_wait2_nb.a) != NULL)
	{
		g_wait2_nb.active = 0;
		return 1;
	}
	if(g_wait2_nb.b != NULL && strstr(Uart2_Buf, g_wait2_nb.b) != NULL)
	{
		g_wait2_nb.active = 0;
		return 2;
	}
	if(strstr(Uart2_Buf, "ERROR") != NULL)
	{
		g_wait2_nb.active = 0;
		return 3;
	}
	if((u32)(Bare_GetTickMs() - g_wait2_nb.t0) >= g_wait2_nb.tmo_ms)
	{
		g_wait2_nb.active = 0;
		return 3;
	}
	ESP8266_CooperativeYield();
	return 0;
}

/* ---------- 连接 WiFi（成功打印 IP）；MQTT 仅通过 AT+MQTT ---------- */
u8 ESP8266_Connect_WiFi(void)
{
		if(!ESP8266_WiFi_Join())
		{
				g_wifi_fail_reason = 2u;
				UART1_SendStr("[WiFi] CWMODE or CWJAP fail\r\n");
				UART1_SendStr("[OneNET] WiFi连接失败，初始化终止\r\n");
				return 0;
		}
		g_wifi_fail_reason = 0u;
		g_wifi_err = 0u;
#if BOARD_WIFI_PRECONFIGURED
		UART1_SendStr("[WiFi] preconf: STA IP OK (CIFSR in join)\r\n");
		return 1;
#else
		CLR_Buf2();
		UART2_SendString("AT+CIFSR");
		UART2_SendString("\r\n");
		if(WaitSubstr("OK",3000))
		{
				UART1_SendStr("WiFi连接成功，IP信息:\r\n");
				UART1_SendStr(Uart2_Buf);
				UART1_SendStr("\r\n");
		}
		else
				UART1_SendStr("WiFi连接成功（CIFSR超时，未读到IP）\r\n");
		return 1;
#endif
}

u8 ESP8266_TLS_Init(void)
{
		ESP8266_Online_Flag = 0;
		g_wifi_fail_reason = 0u;
		g_wifi_substage = 0u;
		g_wifi_err = 0u;
		ESP_DELAY_MS((u16)ESP_AT_BOOT_DELAY_MS);
		g_net_stage = 1; /* AT */
		g_wifi_substage = 1u;
		{
				u8 n;
				for(n = 0u; n < (u8)ESP_AT_HANDSHAKE_RETRIES; n++)
				{
						if(ESP8266_AT_SendWait("AT","OK",1000))
								break;
						ESP8266_CooperativeYield();
						ESP_DELAY_MS(120);
				}
				if(n >= (u8)ESP_AT_HANDSHAKE_RETRIES)
				{
						g_wifi_fail_reason = 1u;
						UART1_SendStr("[OneNET] 模块AT握手失败，初始化终止\r\n");
						UART1_SendStr("[OneNET] last rsp:\r\n");
						UART1_SendStr(Uart2_Buf);
						UART1_SendStr("\r\n");
						return 0;
				}
		}
		ESP8266_AT_SendWait("ATE0","OK",1000);
		ESP8266_AT_SendWait("AT+CIPMUX=0","OK",1000);
		ESP8266_Debug_DumpBootInfo();
		g_net_stage = 2; /* WIFI */
		g_wifi_substage = 20u;
		if(!ESP8266_Connect_WiFi()) return 0;
		g_wifi_substage = 23u;
		return 1;
}

/*
 * 全链路初始化（同步封装）：内部仅轮询非阻塞状态机直至完成。
 */
u8 ESP8266_OneNET_Full_Init(void)
{
		ESP8266_OneNET_InitFsm_Reset();
		while(!tls_inited)
				ESP8266_OneNET_InitFsm_Poll();
		return 1;
}

/* ---------- OneNET/ESP 非阻塞主状态机（MCU 上电节拍 → AT → WiFi → MQTT） ---------- */
static u16 s_onenet_mst;
static u32 s_onenet_t0;
static u8 s_tls_hs_n;

void ESP8266_OneNET_InitFsm_Reset(void)
{
		s_onenet_mst = 0;
		s_tls_hs_n = 0;
		tls_inited = 0;
		g_at_nb.active = 0;
		g_wait_nb.active = 0;
		g_wait2_nb.active = 0;
		ESP8266_OneNET_MqttFsm_Reset();
}

void ESP8266_OneNET_InitFsm_Poll(void)
{
		u32 now = Bare_GetTickMs();
		u8 pr;

#if !ONENET_MQTT_ENABLE
		tls_inited = 1;
		g_net_stage = 4;
		(void)now;
		return;
#endif
		if(tls_inited)
				return;

		switch(s_onenet_mst)
		{
		case 0:
				s_onenet_t0 = now;
				s_onenet_mst = 1;
				return;
		case 1:
				if((u32)(now - s_onenet_t0) < 200u)
						return;
				s_onenet_t0 = now;
				s_onenet_mst = 2;
				return;
		case 2:
				if((u32)(now - s_onenet_t0) < (u32)ESP_AT_BOOT_DELAY_MS)
						return;
				g_net_reconnect_count++;
				Net_LogSimple("[NET] reconnect begin");
				ESP8266_Online_Flag = 0;
				g_wifi_fail_reason = 0u;
				g_wifi_substage = 0u;
				g_wifi_err = 0u;
				g_net_stage = 1;
				g_wifi_substage = 1u;
				s_tls_hs_n = 0;
				ESP8266_AT_Nb_Begin("AT", "OK", 1000);
				s_onenet_mst = 11;
				return;
		case 11:
				pr = ESP8266_AT_Nb_Poll();
				if(pr == 0)
						return;
				if(pr == 1)
				{
						s_onenet_mst = 20;
						return;
				}
				s_tls_hs_n++;
				if(s_tls_hs_n >= (u8)ESP_AT_HANDSHAKE_RETRIES)
				{
						g_wifi_fail_reason = 1u;
						Net_MarkOffline("AT handshake failed");
						UART1_SendStr("[OneNET] 模块AT握手失败(FSM)\r\n");
						ESP8266_RESET();
						s_onenet_mst = 0;
						s_onenet_t0 = now;
						return;
				}
				s_onenet_t0 = now;
				s_onenet_mst = 12;
				return;
		case 12:
				if((u32)(now - s_onenet_t0) < 120u)
						return;
				ESP8266_AT_Nb_Begin("AT", "OK", 1000);
				s_onenet_mst = 11;
				return;
		case 20:
				ESP8266_AT_Nb_Begin("ATE0", "OK", 1000);
				s_onenet_mst = 21;
				return;
		case 21:
				pr = ESP8266_AT_Nb_Poll();
				if(pr == 0)
						return;
				if(pr != 1)
				{
						ESP8266_RESET();
						s_onenet_mst = 0;
						s_onenet_t0 = now;
						return;
				}
				s_onenet_mst = 30;
				return;
		case 30:
				ESP8266_AT_Nb_Begin("AT+CIPMUX=0", "OK", 1000);
				s_onenet_mst = 31;
				return;
		case 31:
				pr = ESP8266_AT_Nb_Poll();
				if(pr == 0)
						return;
				if(pr != 1)
				{
						ESP8266_RESET();
						s_onenet_mst = 0;
						s_onenet_t0 = now;
						return;
				}
				g_net_stage = 2;
				g_wifi_substage = 20u;
				s_onenet_mst = 40;
				return;
		case 40:
				if(!ESP8266_Connect_WiFi())
				{
						g_wifi_fail_reason = 2u;
						Net_MarkOffline("WiFi join failed");
						UART1_SendStr("[OneNET] WiFi连接失败(FSM)\r\n");
						ESP8266_RESET();
						s_onenet_mst = 0;
						s_onenet_t0 = now;
						return;
				}
				g_net_stage = 3;
				ESP8266_OneNET_MqttFsm_Reset();
				s_onenet_mst = 50;
				return;
		case 50:
		{
				u8 mr = ESP8266_OneNET_MqttFsm_Poll();
				if(mr == 0)
						return;
				if(mr == 2)
				{
						g_wifi_fail_reason = 3u;
						Net_MarkOffline("MQTT connect failed");
						UART1_SendStr("[OneNET] MQTT连接失败(FSM)\r\n");
						ESP8266_RESET();
						ESP8266_OneNET_MqttFsm_Reset();
						s_onenet_mst = 0;
						s_onenet_t0 = now;
						return;
				}
				tls_inited = 1;
				g_wifi_fail_reason = 0u;
				g_net_stage = 4;
				Net_LogSimple("[NET] reconnect success");
				UART1_SendStr("[OneNET] WiFi与MQTT鉴权成功(FSM)\r\n");
				s_onenet_mst = 100;
				return;
		}
		case 100:
		default:
				return;
		}
}

/* ---------- 物模型属性上报：JSON + AT+MQTTPUBRAW（无裸 MQTT 二进制） ---------- */
void OneNET_Publish_Data(u8 temp, u8 humi, u16 gas, u8 door, u8 arm, u8 alarm, u8 led)
{
		char topic[120];
		char jb[384];
		static unsigned id_n = 1;

#if !ONENET_MQTT_ENABLE
		(void)temp; (void)humi; (void)gas; (void)door; (void)arm; (void)alarm; (void)led;
		return;
#else
		if(ESP8266_Online_Flag == 0) return;

		sprintf(topic, "$sys/%s/%s/thing/property/post",
						ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
		/*
		 * 物模型标识符需与 OneNET 控制台「功能定义」一致（截图）：
		 * temp/hum/gas 为 float；door/arm/alarm/led 为 bool。
		 */
		sprintf(jb,
				"{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
				"\"temp\":{\"value\":%.1f},\"hum\":{\"value\":%.1f},\"gas\":{\"value\":%.1f},"
				"\"door\":{\"value\":%s},\"arm\":{\"value\":%s},\"alarm\":{\"value\":%s},\"led\":{\"value\":%s}"
				"}}",
				id_n++,
				(float)temp, (float)humi, (float)gas,
				door ? "true" : "false",
				arm ? "true" : "false",
				alarm ? "true" : "false",
				led ? "true" : "false");

		/* 发布失败会在 OneNET_AT_Mqtt_PublishRaw 内部拉低 tls_inited，触发上层自动重连 */
		OneNET_AT_Mqtt_PublishRaw(topic, jb, (u16)strlen(jb));
#endif
}

/* ---------- 解析平台下行（Uart2_Buf 中 JSON，含 AT+MQTT 上报文） ---------- */
void OneNET_Parse_Cmd(void)
{
		char *p;
		unsigned v;
		const char *scan = Uart2_Buf;
		unsigned cmd_id = 0u;
		unsigned cmd_ts = 0u;
		static unsigned last_cmd_id = 0u;
		static unsigned last_cmd_ts = 0u;

		if(Uart2_Buf[0] == 0) return;

		/* ESP-AT MQTT 下行常为 +MQTTSUBRECV:...JSON...；物模型 JSON 仍以 { 为锚点解析 */
		p = strstr(Uart2_Buf, "+MQTTSUBRECV:");
		if(p != NULL)
				scan = p;

		/* 下行命令幂等：基于 cmdId + ts 去重，避免重连后重复执行。 */
		p = strstr(scan, "\"cmdId\"");
		if(p != NULL)
		{
				if(sscanf(p, "\"cmdId\":%u", &cmd_id) == 1)
				{
						char *pt = strstr(scan, "\"ts\"");
						if(pt != NULL) (void)sscanf(pt, "\"ts\":%u", &cmd_ts);
						if(cmd_id == last_cmd_id && cmd_ts == last_cmd_ts)
						{
								Net_LogSimple("[NET] dedup duplicate cmd");
								return;
						}
						last_cmd_id = cmd_id;
						last_cmd_ts = cmd_ts;
				}
		}

		UART1_SendStr("收到平台下发指令:\r\n");
		UART1_SendStr(Uart2_Buf);
		UART1_SendStr("\r\n");

		if((p = strstr(scan, "\"led\"")) != NULL)
		{
				if(sscanf(p, "\"led\":{\"value\":%u", &v) == 1)
						Ctrl_Led = (u8)v;
				else if(sscanf(p, "\"led\":%u", &v) == 1)
						Ctrl_Led = (u8)v;
				else if(strstr(p, "\"led\":{\"value\":true") != NULL) Ctrl_Led = 1;
				else if(strstr(p, "\"led\":{\"value\":false") != NULL) Ctrl_Led = 0;
		}
		if((p = strstr(scan, "ctrl_door")) != NULL)
		{
				if(sscanf(p, "ctrl_door\":{\"value\":%u", &v) == 1)
						Ctrl_Door = (u8)v;
				else if(sscanf(p, "ctrl_door\":%u", &v) == 1)
						Ctrl_Door = (u8)v;
				else if(strstr(p, "ctrl_door\":{\"value\":true") != NULL) Ctrl_Door = 1;
				else if(strstr(p, "ctrl_door\":{\"value\":false") != NULL) Ctrl_Door = 0;
				else if(strstr(p, "true") != NULL) Ctrl_Door = 1;
				else if(strstr(p, "false") != NULL) Ctrl_Door = 0;
		}
		if((p = strstr(scan, "ctrl_arm")) != NULL)
		{
				if(sscanf(p, "ctrl_arm\":{\"value\":%u", &v) == 1)
						Ctrl_Arm = (u8)v;
				else if(sscanf(p, "ctrl_arm\":%u", &v) == 1)
						Ctrl_Arm = (u8)v;
				else if(strstr(p, "ctrl_arm\":{\"value\":true") != NULL) Ctrl_Arm = 1;
				else if(strstr(p, "ctrl_arm\":{\"value\":false") != NULL) Ctrl_Arm = 0;
		}
}
