#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*
 * FreeRTOS 运行前提（仅改本头文件时须保持与下列工程事实一致）：
 * - 内核节拍时钟：与 SystemCoreClock 相同，当前为 72 MHz（system_stm32f10x.c 中 SYSCLK_FREQ_72MHz）。
 * - SysTick：1 kHz（configTICK_RATE_HZ）；stm32f10x_it.c 的 SysTick_Handler 须调用 xPortSysTickHandler()，
 *   并与 g_bare_tick_ms 维护策略一致（见 .cursorrules）。
 * - 堆（heap_4）：configTOTAL_HEAP_SIZE 为任务栈、TCB、队列、互斥量等 pvPortMalloc 分配的总池大小。
 * - 中断与 NVIC 分组由应用代码配置；在 ISR 中调用 FreeRTOS API 时须遵守 Cortex-M3 优先级与
 *   configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 的约束。
 */

#include "stm32f10x.h"

/* 违反调度器假设时关中断并停住，便于联调（不增加其它 .c 文件） */
#define configASSERT( x )    \
    do                       \
    {                        \
        if( ( x ) == 0 )     \
        {                    \
            __disable_irq(); \
            for( ;; )        \
                ;            \
        }                    \
    } while( 0 )

#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                    ( 8 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 24 * 1024 ) )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configQUEUE_REGISTRY_SIZE               0
#define configCHECK_FOR_STACK_OVERFLOW          1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_COUNTING_SEMAPHORES           0

#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

#define configUSE_TIMERS                        0

#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1

#define xPortPendSVHandler PendSV_Handler
#define vPortSVCHandler SVC_Handler

#endif
