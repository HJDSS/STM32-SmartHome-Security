#ifndef __SYSLOG_H
#define __SYSLOG_H

#include "sys.h"

// -------- Flash循环日志存储区(避开secure_store使用的0x0801FF00页) --------
#define SYSLOG_FLASH_BASE        0x0801E000   // 2KB对齐页起始(按需调整)
#define SYSLOG_FLASH_PAGES       4            // 4页=8KB
#define SYSLOG_PAGE_SIZE         2048
#define SYSLOG_SLOT_SIZE         32           // 每条记录32字节(固定)
#define SYSLOG_MAX_SLOTS         ((SYSLOG_FLASH_PAGES*SYSLOG_PAGE_SIZE)/SYSLOG_SLOT_SIZE)

typedef enum
{
		LOG_EVT_UNLOCK = 1,
		LOG_EVT_ALARM  = 2,
		LOG_EVT_CONFIG = 3,
}LOG_EVT_T;

typedef enum
{
		LOG_FILTER_ALL    = 0,
		LOG_FILTER_UNLOCK = LOG_EVT_UNLOCK,
		LOG_FILTER_ALARM  = LOG_EVT_ALARM,
		LOG_FILTER_CONFIG = LOG_EVT_CONFIG,
}LOG_FILTER_T;

typedef struct
{
		u32 seq;
		u8  type;
		u8  rsv0;
		u16 rsv1;
		u32 tick;
		char detail[20]; // 事件详情(截断)
}SYSLOG_ITEM_T;

u8 SysLog_Init(void);
u8 SysLog_Add(LOG_EVT_T type,const char *detail);
u8 SysLog_ReadByIndex(u16 index,SYSLOG_ITEM_T *item); // index:0=最新
u16 SysLog_Count(void);

void SysLog_OLED_ShowPage(u16 page,LOG_FILTER_T filter);
void SysLog_ReportLast(u8 n,LOG_FILTER_T filter);
void SysLog_Cmd_Proc(char *cmd);

#endif

