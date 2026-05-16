#include "stm32f10x_it.h"
#include "sys.h"
#include "oled.h"
#include "gpio.h"

extern volatile u32 g_bare_tick_ms;
extern void xPortSysTickHandler(void);


void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
  OLED_Clear();
  OLED_ShowString(0,0,"HARD FAULT",16);
  OLED_ShowString(0,16,"check stack/ptr",16);
  BEEP_SoundOn();
  while (1) {}
}

void MemManage_Handler(void)
{
  while (1)
  {
  }
}

void BusFault_Handler(void)
{
  while (1)
  {
  }
}

void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

void DebugMon_Handler(void)
{
}

void SysTick_Handler(void)
{
    g_bare_tick_ms++;
#if USE_FREERTOS
    xPortSysTickHandler();
#endif
}
