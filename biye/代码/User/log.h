#ifndef __APP_LOG_H
#define __APP_LOG_H

#include <stdio.h>
#include "board_config.h"

/*
 * 统一日志标签与开关（P1）
 * - 走 printf（最终由 usart1.c 的 fputc 输出）
 * - 可按 tag 单独开关
 */

#ifndef APP_LOG_ENABLE
#define APP_LOG_ENABLE  1
#endif

#ifndef APP_LOG_NET
#define APP_LOG_NET     1
#endif
#ifndef APP_LOG_SD
#define APP_LOG_SD      1
#endif
#ifndef APP_LOG_SENSOR
#define APP_LOG_SENSOR  1
#endif
#ifndef APP_LOG_ACT
#define APP_LOG_ACT     1
#endif
#ifndef APP_LOG_STAT
#define APP_LOG_STAT    1
#endif

#if APP_LOG_ENABLE
#define APP_LOG_PRINTF(tag, fmt, ...) \
    do { printf("[%s] " fmt "\r\n", (tag), ##__VA_ARGS__); } while(0)
#else
#define APP_LOG_PRINTF(tag, fmt, ...) do { (void)(tag); } while(0)
#endif

#if APP_LOG_NET
#define LOG_NET(fmt, ...)    APP_LOG_PRINTF("NET", fmt, ##__VA_ARGS__)
#else
#define LOG_NET(fmt, ...)    do {} while(0)
#endif

#if APP_LOG_SD
#define LOG_SD(fmt, ...)     APP_LOG_PRINTF("SD", fmt, ##__VA_ARGS__)
#else
#define LOG_SD(fmt, ...)     do {} while(0)
#endif

#if APP_LOG_SENSOR
#define LOG_SENSOR(fmt, ...) APP_LOG_PRINTF("SENSOR", fmt, ##__VA_ARGS__)
#else
#define LOG_SENSOR(fmt, ...) do {} while(0)
#endif

#if APP_LOG_ACT
#define LOG_ACT(fmt, ...)    APP_LOG_PRINTF("ACT", fmt, ##__VA_ARGS__)
#else
#define LOG_ACT(fmt, ...)    do {} while(0)
#endif

#if APP_LOG_STAT
#define LOG_STAT(fmt, ...)   APP_LOG_PRINTF("STAT", fmt, ##__VA_ARGS__)
#else
#define LOG_STAT(fmt, ...)   do {} while(0)
#endif

#endif

