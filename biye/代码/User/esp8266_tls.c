#include "esp8266_tls.h"
#include "esp8266_onenet_mqtt.h"
#include "board_config.h"
#include "usart2.h"
#include "delay.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "app_params.h"
#include "gpio.h"
#include "linkage.h"
#include "lock_manager.h"

extern volatile u8 RELAY_TIME;
extern volatile u32 g_last_cmd_received_tick;
extern volatile u32 g_last_cmd_completed_tick;

#define ESP_DELAY_MS(ms) delay_ms((u16)(ms))

extern void uart1_SendStr(char *SendBuf);
extern void Security_Set_Mode(u8 armed);
#define UART1_SendStr uart1_SendStr

#define WIFI_SSID    BOARD_WIFI_SSID
#define WIFI_PASS    BOARD_WIFI_PASS
#ifndef WIFI_CWJAP_TIMEOUT_MS
#define WIFI_CWJAP_TIMEOUT_MS  20000u
#endif
#ifndef WIFI_CWJAP_RETRY_COUNT
#define WIFI_CWJAP_RETRY_COUNT  2u
#endif
#ifndef ESP_AT_BOOT_DELAY_MS
#define ESP_AT_BOOT_DELAY_MS   1200u
#endif
#ifndef ESP_AT_HANDSHAKE_RETRIES
#define ESP_AT_HANDSHAKE_RETRIES  5u
#endif
#ifndef WIFI_PRECONF_CIFSR_RETRIES
#define WIFI_PRECONF_CIFSR_RETRIES  50u
#endif
#ifndef WIFI_PRECONF_CIFSR_GAP_MS
#define WIFI_PRECONF_CIFSR_GAP_MS   400u
#endif
#ifndef WIFI_PRECONF_CIFSR_POST_OK_MS
#define WIFI_PRECONF_CIFSR_POST_OK_MS  60u
#endif
#ifndef WIFI_MAX_CHANNEL
#define WIFI_MAX_CHANNEL  11u
#endif
#ifndef WIFI_USE_CWLAP_PRECHECK
#define WIFI_USE_CWLAP_PRECHECK  0u
#endif

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

u8 Ctrl_Led;
u8 Ctrl_Door;
u8 Ctrl_Arm;
u8 ESP8266_Online_Flag;

u8 tls_inited = 0;
volatile u8 g_net_stage = 0;
volatile u8 g_wifi_fail_reason = 0;
volatile u8 g_wifi_substage = 0;
volatile u8 g_wifi_err = 0;
volatile u32 g_net_reconnect_count = 0;
volatile u32 g_net_offline_count = 0;

static char s_payload_snap[512];
static char s_reply_id[64];
static u8 s_is_property_set;
static u8 s_last_ctrl_door = 0u;
static u8 s_last_ctrl_led = 0u;

static u8 WaitSubstr(const char *sub, u16 timeout_ms);
static void ESP8266_Debug_DumpBootInfo(void);
static void Net_LogSimple(const char *msg);
static void Net_MarkOffline(const char *reason);

static void OneNET_Reply_PropertySet(u8 code)
{
    char topic[120];
    char jb[128];

    if(s_reply_id[0] == '\0') return;
    sprintf(topic, "$sys/%s/%s/thing/property/set/reply",
            ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    sprintf(jb, "{\"id\":\"%s\",\"code\":%u}", s_reply_id, (unsigned)code);
    OneNET_AT_Mqtt_PublishRaw(topic, jb, (u16)strlen(jb));
}

static void OneNET_Process_Remote_Ctrl(void)
{
    if(Ctrl_Door != s_last_ctrl_door)
    {
        if(Ctrl_Door != 0u)
        {
            /* 🟡1: 远程开锁前检查本地安全状态 */
            if(LockManager_IsPwdLocked() || LockManager_IsBruteAlarm())
            {
                Net_LogSimple("[NET] remote unlock denied: locked/brute");
            }
            else
            {
                RELAY = 0;
                RELAY_TIME = APP_LOCK_OPEN_HOLD_TICKS;
                Linkage_OnUnlock(UNLOCK_SRC_REMOTE);
            }
        }
        s_last_ctrl_door = Ctrl_Door;
    }
    if(Ctrl_Led != s_last_ctrl_led)
    {
        if(Ctrl_Led != 0u)
            Light_Relay_On();
        else
            Light_Relay_Off();
        s_last_ctrl_led = Ctrl_Led;
    }
}

static void Net_LogSimple(const char *msg)
{
    if(msg == NULL) return;
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

void ESP8266_EN_GPIO_Init(void)
{
#if BOARD_WIFI_USE_NODEMCU
    return;
#else
    GPIO_InitTypeDef gi;
    RCC_APB2PeriphClockCmd(BOARD_ESP8266_EN_CLK, ENABLE);
    GPIO_WriteBit(BOARD_ESP8266_EN_PORT, BOARD_ESP8266_EN_PIN, Bit_SET);
    gi.GPIO_Pin   = BOARD_ESP8266_EN_PIN;
    gi.GPIO_Mode  = GPIO_Mode_Out_PP;
    gi.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(BOARD_ESP8266_EN_PORT, &gi);
    GPIO_WriteBit(BOARD_ESP8266_EN_PORT, BOARD_ESP8266_EN_PIN, Bit_SET);
#endif
}

void ESP8266_RESET(void)
{
#if BOARD_WIFI_USE_NODEMCU
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

static u8 ESP8266_WiFi_Join(void)
{
#if BOARD_WIFI_PRECONFIGURED
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
                if(strstr(Uart2_Buf, "+CIFSR") != NULL && cifsr_buf_has_sta_ip(Uart2_Buf))
                    return 1;
            }
            ESP8266_CooperativeYield();
            ESP_DELAY_MS((u16)WIFI_PRECONF_CIFSR_GAP_MS);
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
    CLR_Buf2();
    UART2_SendString("AT+CWMODE?\r\n");
    if(!(WaitSubstr("OK", 2500) && (strstr(Uart2_Buf, "+CWMODE:1") != NULL || strstr(Uart2_Buf, "+CWMODE_CUR:1") != NULL)))
    {
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
    g_wifi_substage = 21u;
    ESP8266_AT_SendWait("AT+CWQAP", "OK", 2000);
    for(n = 0u; n < (u8)WIFI_CWJAP_RETRY_COUNT; n++)
    {
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
    g_net_stage = 1;
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
    g_net_stage = 2;
    g_wifi_substage = 20u;
    if(!ESP8266_Connect_WiFi()) return 0;
    g_wifi_substage = 23u;
    return 1;
}

u8 ESP8266_OneNET_Full_Init(void)
{
    ESP8266_OneNET_InitFsm_Reset();
    while(!tls_inited)
        ESP8266_OneNET_InitFsm_Poll();
    return 1;
}

static u16 s_onenet_mst;
static u32 s_onenet_t0;
static u32 s_backoff_ms = APP_NET_BACKOFF_INIT_MS;
static u8 s_tls_hs_n;

void ESP8266_OneNET_InitFsm_Reset(void)
{
    s_onenet_mst = 0;
    s_tls_hs_n = 0;
    s_backoff_ms = APP_NET_BACKOFF_INIT_MS;
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
        if((u32)(now - s_onenet_t0) < s_backoff_ms)
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
            s_backoff_ms = s_backoff_ms * 2;
            if (s_backoff_ms > APP_NET_BACKOFF_MAX_MS) s_backoff_ms = APP_NET_BACKOFF_MAX_MS;
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
            s_backoff_ms = s_backoff_ms * 2;
            if (s_backoff_ms > APP_NET_BACKOFF_MAX_MS) s_backoff_ms = APP_NET_BACKOFF_MAX_MS;
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
            s_backoff_ms = s_backoff_ms * 2;
            if (s_backoff_ms > APP_NET_BACKOFF_MAX_MS) s_backoff_ms = APP_NET_BACKOFF_MAX_MS;
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
            s_backoff_ms = s_backoff_ms * 2;
            if (s_backoff_ms > APP_NET_BACKOFF_MAX_MS) s_backoff_ms = APP_NET_BACKOFF_MAX_MS;
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
            s_backoff_ms = s_backoff_ms * 2;
            if (s_backoff_ms > APP_NET_BACKOFF_MAX_MS) s_backoff_ms = APP_NET_BACKOFF_MAX_MS;
            ESP8266_RESET();
            ESP8266_OneNET_MqttFsm_Reset();
            s_onenet_mst = 0;
            s_onenet_t0 = now;
            return;
        }
        tls_inited = 1;
        s_backoff_ms = APP_NET_BACKOFF_INIT_MS;
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
    sprintf(jb,
            "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
            "\"temp\":{\"value\":%.1f},\"hum\":{\"value\":%.1f},\"gas\":{\"value\":%.1f},"
            "\"door\":{\"value\":%s},\"arm\":{\"value\":%s},\"ctrl_arm\":{\"value\":%s},\"alarm\":{\"value\":%s},\"led\":{\"value\":%s}"
            "}}",
            id_n++,
            (float)temp, (float)humi, (float)gas,
            door ? "true" : "false",
            arm ? "true" : "false",
            arm ? "true" : "false",
            alarm ? "true" : "false",
            led ? "true" : "false");

    OneNET_AT_Mqtt_PublishRaw(topic, jb, (u16)strlen(jb));
#endif
}

/* 🟡10: 异常告警即时推送 — 独立于5秒周期上报，事件驱动 */
void OneNET_Publish_Alarm(const char *alarm_type)
{
#if ONENET_MQTT_ENABLE
    char topic[120];
    char payload[256];
    u32 tick;
    if (alarm_type == NULL) return;
    if (ESP8266_Online_Flag == 0u) return;
    tick = (u32)Bare_GetTickMs();
    sprintf(topic, "$sys/%s/%s/thing/property/post",
            ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    sprintf(payload,
            "{\"id\":\"alarm%lu\",\"version\":\"1.0\",\"params\":{\"alarm_event\":{\"value\":\"%s\"},\"alarm_ts\":{\"value\":%lu}}}",
            (unsigned long)tick, alarm_type, (unsigned long)tick);
    OneNET_AT_Mqtt_PublishRaw(topic, payload, (u16)strlen(payload));
    LOG_NET("alarm push: %s", alarm_type);
#else
    (void)alarm_type;
#endif
}

u8 OneNET_PropertySetPending(void)
{
    return s_is_property_set;
}

const char *OneNET_PropertySetPayload(void)
{
    return s_payload_snap;
}

const char *OneNET_PropertySetReplyId(void)
{
    return s_reply_id;
}

void OneNET_PropertySetClear(void)
{
    s_is_property_set = 0u;
}

void OneNET_Parse_Cmd(void)
{
#if !ONENET_MQTT_ENABLE
    return;
#else
    char *p;
    unsigned v;
    u8 cmd_arm = 0u;
    u8 cmd_arm_found = 0u;
    const char *scan = Uart2_Buf;
    const char *param_src;
    unsigned cmd_id = 0u;
    unsigned cmd_ts = 0u;
    /* 🟡3: 环形缓冲区去重替代单槽, APP_CMD_DEDUP_RING=16, APP_CMD_STALE_MS=60s */
    static unsigned s_dedup_id[APP_CMD_DEDUP_RING];
    static unsigned s_dedup_ts[APP_CMD_DEDUP_RING];
    static u8 s_dedup_head = 0u;
    static u8 s_dedup_count = 0u;
    unsigned i;

    if(Uart2_Buf[0] == 0) return;

    p = strstr(Uart2_Buf, "+MQTTSUBRECV:");
    if(p != NULL)
        scan = p;

    p = strstr((char *)scan, "\"cmdId\"");
    if(p != NULL)
    {
        if(sscanf(p, "\"cmdId\":%u", &cmd_id) == 1)
        {
            char *pt = strstr((char *)scan, "\"ts\"");
            if(pt != NULL) (void)sscanf(pt, "\"ts\":%u", &cmd_ts);

            /* 🟡3: 环冲去重 — 扫描已有条目，匹配则拒绝 */
            {
                unsigned now_ts = (unsigned)Bare_GetTickMs();
                u8 j;
                for (j = 0u; j < s_dedup_count; j++)
                {
                    u8 idx = (u8)((s_dedup_head + APP_CMD_DEDUP_RING - s_dedup_count + j) % APP_CMD_DEDUP_RING);
                    if ((u32)(now_ts - s_dedup_ts[idx]) > (u32)APP_CMD_STALE_MS)
                        continue; /* 过期条目跳过 */
                    if (s_dedup_id[idx] == cmd_id && s_dedup_ts[idx] == cmd_ts)
                    {
                        Net_LogSimple("[NET] dedup duplicate cmd");
                        return;
                    }
                }
                /* 写入环冲 */
                s_dedup_id[s_dedup_head] = cmd_id;
                s_dedup_ts[s_dedup_head] = (unsigned)Bare_GetTickMs();
                s_dedup_head = (u8)((s_dedup_head + 1u) % APP_CMD_DEDUP_RING);
                if (s_dedup_count < APP_CMD_DEDUP_RING)
                    s_dedup_count++;
            }
        }
    }

    g_last_cmd_received_tick = Bare_GetTickMs();

    s_is_property_set = 0u;
    s_reply_id[0] = '\0';
    s_payload_snap[0] = '\0';

    p = strstr((char *)scan, "property/set");
    if(p != NULL)
    {
        char *json_start = strchr(p, '{');
        if(json_start != NULL)
        {
            for(i = 0u; i < (unsigned)(sizeof(s_payload_snap) - 1u) && json_start[i] != 0; i++)
                s_payload_snap[i] = json_start[i];
            s_payload_snap[i] = '\0';

            p = strstr(s_payload_snap, "\"id\":\"");
            if(p != NULL)
            {
                char *qs = p + 6;
                char *qe = strchr(qs, '"');
                if(qe != NULL && qe > qs
                    && (size_t)(qe - qs) < sizeof(s_reply_id))
                {
                    (void)memcpy(s_reply_id, qs, (size_t)(qe - qs));
                    s_reply_id[(size_t)(qe - qs)] = '\0';
                    s_is_property_set = 1u;
                }
            }
            else
            {
                char *qe;
                p = strstr(s_payload_snap, "\"id\":");
                if(p != NULL)
                {
                    char *qs = p + 5;
                    char *endp = NULL;
                    unsigned long n;

                    if(*qs == '"')
                    {
                        qs++;
                        qe = strchr(qs, '"');
                        if(qe != NULL && qe > qs
                            && (size_t)(qe - qs) < sizeof(s_reply_id))
                        {
                            (void)memcpy(s_reply_id, qs, (size_t)(qe - qs));
                            s_reply_id[(size_t)(qe - qs)] = '\0';
                            s_is_property_set = 1u;
                        }
                    }
                    else
                    {
                        n = strtoul(qs, &endp, 10);
                        if(endp != qs)
                        {
                            (void)snprintf(s_reply_id, sizeof(s_reply_id), "%lu", n);
                            s_is_property_set = 1u;
                        }
                    }
                }
            }
        }
    }

    param_src = (s_payload_snap[0] != 0) ? (const char *)s_payload_snap : scan;

    UART1_SendStr("收到云端数据:\r\n");
    UART1_SendStr(Uart2_Buf);
    UART1_SendStr("\r\n");

    if((p = strstr((char *)param_src, "\"led\"")) != NULL)
    {
        if(sscanf(p, "\"led\":{\"value\":%u", &v) == 1)
            Ctrl_Led = (u8)v;
        else if(sscanf(p, "\"led\":%u", &v) == 1)
            Ctrl_Led = (u8)v;
        else if(strstr(p, "\"led\":{\"value\":\"true\"") != NULL) Ctrl_Led = 1;
        else if(strstr(p, "\"led\":{\"value\":\"false\"") != NULL) Ctrl_Led = 0;
        else if(strstr(p, "\"led\":{\"value\":true") != NULL) Ctrl_Led = 1;
        else if(strstr(p, "\"led\":{\"value\":false") != NULL) Ctrl_Led = 0;
    }
    if((p = strstr((char *)param_src, "ctrl_door")) != NULL)
    {
        if(sscanf(p, "ctrl_door\":{\"value\":%u", &v) == 1)
            Ctrl_Door = (u8)v;
        else if(sscanf(p, "ctrl_door\":%u", &v) == 1)
            Ctrl_Door = (u8)v;
        else if(strstr(p, "ctrl_door\":{\"value\":\"true\"") != NULL) Ctrl_Door = 1;
        else if(strstr(p, "ctrl_door\":{\"value\":\"false\"") != NULL) Ctrl_Door = 0;
        else if(strstr(p, "ctrl_door\":{\"value\":true") != NULL) Ctrl_Door = 1;
        else if(strstr(p, "ctrl_door\":{\"value\":false") != NULL) Ctrl_Door = 0;
        else if(strstr(p, "true") != NULL) Ctrl_Door = 1;
        else if(strstr(p, "false") != NULL) Ctrl_Door = 0;
    }
    OneNET_Process_Remote_Ctrl();

    if((p = strstr((char *)param_src, "ctrl_arm")) != NULL)
    {
        if(sscanf(p, "ctrl_arm\":{\"value\":%u", &v) == 1)
        {
            cmd_arm = (u8)v;
            cmd_arm_found = 1u;
        }
        else if(sscanf(p, "ctrl_arm\":%u", &v) == 1)
        {
            cmd_arm = (u8)v;
            cmd_arm_found = 1u;
        }
        else if(strstr(p, "ctrl_arm\":{\"value\":\"true\"") != NULL) { cmd_arm = 1u; cmd_arm_found = 1u; }
        else if(strstr(p, "ctrl_arm\":{\"value\":\"false\"") != NULL) { cmd_arm = 0u; cmd_arm_found = 1u; }
        else if(strstr(p, "ctrl_arm\":{\"value\":true") != NULL) { cmd_arm = 1u; cmd_arm_found = 1u; }
        else if(strstr(p, "ctrl_arm\":{\"value\":false") != NULL) { cmd_arm = 0u; cmd_arm_found = 1u; }
    }
    if(!cmd_arm_found && (p = strstr((char *)param_src, "\"arm\"")) != NULL)
    {
        if(sscanf(p, "\"arm\":{\"value\":%u", &v) == 1)
        {
            cmd_arm = (u8)v;
            cmd_arm_found = 1u;
        }
        else if(sscanf(p, "\"arm\":%u", &v) == 1)
        {
            cmd_arm = (u8)v;
            cmd_arm_found = 1u;
        }
        else if(strstr(p, "\"arm\":{\"value\":\"true\"") != NULL) { cmd_arm = 1u; cmd_arm_found = 1u; }
        else if(strstr(p, "\"arm\":{\"value\":\"false\"") != NULL) { cmd_arm = 0u; cmd_arm_found = 1u; }
        else if(strstr(p, "\"arm\":{\"value\":true") != NULL) { cmd_arm = 1u; cmd_arm_found = 1u; }
        else if(strstr(p, "\"arm\":{\"value\":false") != NULL) { cmd_arm = 0u; cmd_arm_found = 1u; }
    }
    if(cmd_arm_found)
    {
        Ctrl_Arm = cmd_arm;
        Security_Set_Mode(Ctrl_Arm);
    }
    g_last_cmd_completed_tick = Bare_GetTickMs();

    if(s_is_property_set && s_reply_id[0] != '\0')
    {
        OneNET_Reply_PropertySet(0);
        OneNET_PropertySetClear();
    }
#endif
}
