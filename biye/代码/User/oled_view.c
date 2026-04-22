#include "oled_view.h"
#include "oled.h"
#include "board_config.h"
#include "delay.h"
#include "esp8266_tls.h"  /* [M1.7] 读 g_net_stage / g_wifi_substage / g_wifi_err / g_wifi_fail_reason */
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

/* [M1.7] 4 字段阶段名：与 esp8266_tls.c g_net_stage 枚举一致。
 * 每个名字都定长 4 字符（不足补空格），这样 OLED 顶行总长严格 16 字符，
 * 不会出现局部刷新把上一帧残留字符留在屏上。 */
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

/* [M1.7] OLED 联网状态行（顶行，16×16 字体，每行严格 16 字符）。
 *
 * 为什么改成这样：以前未连上时只显示 "NET:INIT"，缺少任何定位信息。
 * 现在把 ESP8266 FSM 四个状态量编码进顶行：
 *   - g_net_stage       : 0=IDLE 1=AT 2=WIFI 3=MQTT 4=OK
 *   - g_wifi_substage   : 1=AT 20=CWMODE 21=CWJAP 22=WAIT_IP 23=OK 210=CWLAP
 *   - g_wifi_err        : 0=无 1=CWLAP找不到SSID 2=信道不支持 3=CWJAP超时 4=CWMODE失败
 *   - g_wifi_fail_reason: 0=无 1=AT握手 2=WiFi连接 3=MQTT连接
 *
 * 输出样例（各行皆严格 16 字符）：
 *   正常进行中：
 *     "NET:AT   S01 E0 "   AT 握手中
 *     "NET:WIFI S20 E0 "   已进 WiFi 阶段，正在设 CWMODE
 *     "NET:WIFI S21 E0 "   正在 CWJAP（加入路由器），最容易卡住的阶段
 *     "NET:WIFI S22 E3 "   CWJAP 超时失败（err=3）
 *     "NET:MQTT S00 E0 "   WiFi 已 OK，正在做 OneNET MQTT 建链
 *     "NET:OK   HB:OK  "   全部成功，心跳也 OK（老版已有）
 *     "NET:OK   HB:--  "   建链成功但心跳超时（老版已有）
 *   已经失败（fail_reason 非 0 时优先展示，让你立刻看出故障分类）：
 *     "NET:ERR AT RST  "   AT 握手失败，已触发 EN 硬复位重试
 *     "NET:ERR WiFi e3 "   WiFi 接入失败（err 码见上）
 *     "NET:ERR MQTT    "   WiFi 通了但 OneNET MQTT 连不上
 *
 * 由于 g_wifi_substage 可能出现 210（CWLAP 自检阶段），显示前做截断 0..99，
 * 保证 "S%02u" 始终占 3 个字符而不把后续字段挤掉。 */
void OLED_View_ShowNetState(u8 wifi_ok, u8 hb_ok)
{
    char line[17];
    u8 sub;
    u8 stage;
    u8 err;
    u8 fail;

    if(wifi_ok)
    {
        OLED_ShowString(0, 0, hb_ok ? "NET:OK HB:OK    " : "NET:OK HB:--    ", 16);
        return;
    }

    fail = g_wifi_fail_reason;
    if(fail != 0u)
    {
        switch(fail)
        {
            case 1u:
                OLED_ShowString(0, 0, "NET:ERR AT RST  ", 16);
                break;
            case 2u:
                err = g_wifi_err;
                if(err > 9u) err = 9u;
                sprintf(line, "NET:ERR WiFi e%u ", (unsigned)err);
                line[16] = '\0';
                OLED_ShowString(0, 0, line, 16);
                break;
            case 3u:
                OLED_ShowString(0, 0, "NET:ERR MQTT    ", 16);
                break;
            default:
                OLED_ShowString(0, 0, "NET:ERR ?       ", 16);
                break;
        }
        return;
    }

    stage = g_net_stage;
    sub   = g_wifi_substage;
    err   = g_wifi_err;
    if(sub > 99u) sub = 99u;  /* 210(CWLAP) 等超出 2 位时截断，保持行宽固定 */
    if(err > 9u)  err = 9u;

    sprintf(line, "NET:%s S%02u E%u ", net_stage_name(stage), (unsigned)sub, (unsigned)err);
    line[16] = '\0';
    OLED_ShowString(0, 0, line, 16);
}

void OLED_View_RefreshDashboard(const sensor_state_t *sensor, const lock_state_t *lock, const esp_state_t *esp)
{
    char line[17];
    static u32 s_frame_count = 0u;
    u32 frame_mod;
    u32 tick_100ms;

    if(sensor == NULL || lock == NULL || esp == NULL)
        return;

    /* [M1.8] 每次刷新 +1；用于快速判定 main 循环是否在跑 */
    s_frame_count++;
    frame_mod  = s_frame_count % 100u;
    tick_100ms = (Bare_GetTickMs() / 100u) % 1000u;

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

    /* [M1.8] 第 3 行嵌入两个心跳计数：
     *   F = OLED_View_RefreshDashboard 调用次数 mod 100（主循环到这一行就 +1）
     *   T = Bare_GetTickMs() / 100 mod 1000（SysTick 毫秒计数 /100）
     * 读数规则：
     *   两者都在变 → main 循环活着、SysTick 正常；FSM 还卡就去查 InitFsm_Poll
     *   只有 F 在变 T 不动 → SysTick 没在跑/被某个 IRQ 抢占；FSM 所有时间条件失效
     *   两者都不动 → main 循环卡在 OLED 刷新之前（上面某个 Tick/Poll 里死循环）
     * 严格 16 字符一行，避免局部残留。 */
    if(lock->armed)
        sprintf(line, "ARMED  F%02lu T%03lu", (unsigned long)frame_mod, (unsigned long)tick_100ms);
    else
        sprintf(line, "DISARM F%02lu T%03lu", (unsigned long)frame_mod, (unsigned long)tick_100ms);
    line[16] = '\0';
    OLED_ShowString(0, 48, line, 16);
    OLED_BatchEnd();
}
