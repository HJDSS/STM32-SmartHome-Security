#ifndef __WDG_H
#define __WDG_H

#include "sys.h"
#include "board_config.h"

typedef enum
{
    WDG_SRC_SENSOR = 0,
    WDG_SRC_ALARM,
    WDG_SRC_NET,
    WDG_SRC_UI,
    WDG_SRC_SYSMON,
    WDG_SRC__COUNT
} wdg_src_t;

#if WDG_SRC__COUNT > BOARD_IWDG_WATCH_SRC_MAX
#error "WDG_SRC__COUNT exceeds BOARD_IWDG_WATCH_SRC_MAX"
#endif

void WDG_Init(void);
void WDG_HwInit(void);
void WDG_Mark(wdg_src_t src);
void WDG_Pump(void);
u8 WDG_LastResetWasIWDG(void);
u8 WDG_GetLastStaleSrc(void);

#endif
