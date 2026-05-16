#include "wdg.h"
#include "delay.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"

#ifndef WDG_IWDG_LSI_HZ
#define WDG_IWDG_LSI_HZ  40000u
#endif

#ifndef WDG_IWDG_PRESCALER_DIV
#define WDG_IWDG_PRESCALER_DIV  256u
#endif
#ifndef WDG_IWDG_PR_REG
#define WDG_IWDG_PR_REG  6u
#endif

static u32 s_src_last_kick_ms[(unsigned)WDG_SRC__COUNT];
static u32 s_src_threshold_ms[(unsigned)WDG_SRC__COUNT];
static u8 s_last_stale_src = 0xFFu;

static u16 wdg_calc_reload(u32 timeout_ms)
{
    u64 num;
    u32 reload;

    num = (u64)timeout_ms * (u64)WDG_IWDG_LSI_HZ;
    reload = (u32)(num / ((u64)WDG_IWDG_PRESCALER_DIV * 1000ull));
    if(reload == 0u)
        reload = 1u;
    if(reload > 4095u)
        reload = 4095u;
    return (u16)reload;
}

void WDG_Init(void)
{
    u32 now;
    u32 t;
    unsigned i;

    now = Bare_GetTickMs();
    t = BOARD_IWDG_TIMEOUT_MS;
    if(t < 2000u)
        t = 2000u;
    for(i = 0u; i < (unsigned)WDG_SRC__COUNT; i++)
    {
        s_src_last_kick_ms[i] = now;
        s_src_threshold_ms[i] = (t * 4u) / 5u;
    }
    s_src_threshold_ms[(unsigned)WDG_SRC_UI] = (t / 2u) > 3000u ? (t / 2u) : 3000u;
    s_src_threshold_ms[(unsigned)WDG_SRC_SYSMON] = (t * 9u) / 10u;
    s_last_stale_src = 0xFFu;
}

void WDG_HwInit(void)
{
#if !BOARD_IWDG_ENABLE
    return;
#else
    u16 reload;
    volatile u32 spin;

    reload = wdg_calc_reload(BOARD_IWDG_TIMEOUT_MS);

    IWDG->KR = (u16)0x5555;
    while(IWDG->SR & IWDG_SR_PVU)
    {
        for(spin = 0u; spin < 1000u; spin++)
        {
        }
    }
    IWDG->PR = (u8)WDG_IWDG_PR_REG;
    while(IWDG->SR & IWDG_SR_PVU)
    {
        for(spin = 0u; spin < 1000u; spin++)
        {
        }
    }
    IWDG->RLR = reload;
    while(IWDG->SR & IWDG_SR_RVU)
    {
        for(spin = 0u; spin < 1000u; spin++)
        {
        }
    }
    IWDG->KR = (u16)0xAAAA;
    IWDG->KR = (u16)0xCCCC;
#endif
}

void WDG_Mark(wdg_src_t src)
{
    unsigned idx;

    idx = (unsigned)src;
    if(idx >= (unsigned)WDG_SRC__COUNT)
        return;
    s_src_last_kick_ms[idx] = Bare_GetTickMs();
}

void WDG_Pump(void)
{
#if !BOARD_IWDG_ENABLE
    return;
#else
    unsigned i;
    u32 now;
    u8 all_fresh;

    now = Bare_GetTickMs();
    all_fresh = 1u;

    for(i = 0u; i < (unsigned)WDG_SRC__COUNT; i++)
    {
        u32 age;

        age = (u32)(now - s_src_last_kick_ms[i]);
        if(age >= s_src_threshold_ms[i])
        {
            all_fresh = 0u;
            if(s_last_stale_src == 0xFFu)
                s_last_stale_src = (u8)i;
        }
    }

    if(all_fresh)
    {
        IWDG->KR = (u16)0xAAAA;
        s_last_stale_src = 0xFFu;
    }
#endif
}

u8 WDG_LastResetWasIWDG(void)
{
    u8 was;

    was = 0u;
    if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
        was = 1u;
    RCC_ClearFlag();
    return was;
}

u8 WDG_GetLastStaleSrc(void)
{
    return s_last_stale_src;
}
