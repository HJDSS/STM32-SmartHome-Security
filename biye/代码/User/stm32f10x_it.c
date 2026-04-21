/**
  ******************************************************************************
  * @file    GPIO/IOToggle/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and peripherals
  *          interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "sys.h"
#include "oled.h"
#include "gpio.h"

extern volatile u32 g_bare_tick_ms;


 
void NMI_Handler(void)
{
}
 
void HardFault_Handler(void)
{
  /* 运行时硬故障可视化，便于与“任务不调度”区分 */
  OLED_Clear();
  OLED_ShowString(0,0,"HARD FAULT",16);
  OLED_ShowString(0,16,"check stack/ptr",16);
  BEEP_SoundOn();
  while (1) {}
}
 
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

 
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}
 
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}
 
/* 由 FreeRTOS port.c 提供 SVC 处理：注释掉本文件以避免重复定义 */
/* void SVC_Handler(void)
{
  vPortSVCHandler();
} */
 
void DebugMon_Handler(void)
{
}
 
/* 由 FreeRTOS port.c 提供 PendSV 处理：注释掉本文件以避免重复定义 */
/* void PendSV_Handler(void)
{
  xPortPendSVHandler();
} */
 
void SysTick_Handler(void)
{
	g_bare_tick_ms++;
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/
