#include "syslog.h"
#include "stmflash.h"
#include "string.h"
#include "stdio.h"
#include "oled.h"
#include "oled_ui.h"
#include "usart1.h"
#include "delay.h"

#define LOG_MAGIC_HI      0xA5A5
#define LOG_MAGIC_LO      0x5A5A
#define LOG_VALID_FLAG    0xA55A

typedef struct
{
		u16 magic_hi;
		u16 magic_lo;
		u32 seq;
		u8  type;
		u8  rsv0;
		u16 len;
		u32 tick;
		u8  detail[20];
		u16 crc;
		u16 valid;
}LOG_REC_T;

static u32 g_log_seq = 0;
static u16 g_log_wr_slot = 0;
static u16 g_log_count = 0;

static u16 crc16_ccitt(const u8 *data,u16 len)
{
		u16 crc=0xFFFF;
		u16 i;
		u8 j;
		for(i=0;i<len;i++)
		{
				crc ^= ((u16)data[i])<<8;
				for(j=0;j<8;j++)
				{
						if(crc & 0x8000) crc = (crc<<1) ^ 0x1021;
						else crc <<= 1;
				}
		}
		return crc;
}

static u32 slot_addr(u16 slot)
{
		return (u32)(SYSLOG_FLASH_BASE + (u32)slot * SYSLOG_SLOT_SIZE);
}

static void flash_read_rec(u16 slot,LOG_REC_T *rec)
{
		STMFLASH_Read(slot_addr(slot),(u16*)rec,(sizeof(LOG_REC_T)+1)/2);
}

static u8 rec_is_valid(const LOG_REC_T *r)
{
		u16 crc = 0;
		if(r->valid != LOG_VALID_FLAG) return 0;
		if(r->magic_hi != LOG_MAGIC_HI || r->magic_lo != LOG_MAGIC_LO) return 0;
		if(r->len > 20) return 0;
		/* crc覆盖: seq/type/len/tick/detail */
		crc = crc16_ccitt((const u8*)&r->seq,4+1+1+2+4+20);
		return (crc == r->crc);
}

static void erase_page_if_needed(u16 slot)
{
		u32 addr = slot_addr(slot);
		u32 page = (addr - SYSLOG_FLASH_BASE) / SYSLOG_PAGE_SIZE;
		u32 page_base = SYSLOG_FLASH_BASE + page * SYSLOG_PAGE_SIZE;

		/* 如果该页首字不是0xFFFF，视为已写入过，准备覆盖时先擦页 */
		if(STMFLASH_ReadHalfWord(page_base) != 0xFFFF)
		{
				FLASH_Unlock();
				FLASH_ErasePage(page_base);
				FLASH_Lock();
		}
}

u8 SysLog_Init(void)
{
		u16 i;
		LOG_REC_T r;
		u32 max_seq = 0;
		u16 max_slot = 0xFFFF;
		u16 cnt = 0;

		for(i=0;i<SYSLOG_MAX_SLOTS;i++)
		{
				flash_read_rec(i,&r);
				if(rec_is_valid(&r))
				{
						cnt++;
						if(r.seq >= max_seq)
						{
								max_seq = r.seq;
								max_slot = i;
						}
				}
		}

		g_log_count = cnt;
		g_log_seq = max_seq;
		if(max_slot == 0xFFFF)
		{
				g_log_wr_slot = 0;
				g_log_seq = 0;
				return 0;
		}
		g_log_wr_slot = (u16)((max_slot + 1) % SYSLOG_MAX_SLOTS);
		return 0;
}

u16 SysLog_Count(void)
{
		return g_log_count;
}

u8 SysLog_Add(LOG_EVT_T type,const char *detail)
{
		LOG_REC_T r;
		u16 i;
		u16 v;
		memset(&r,0,sizeof(r));

		r.magic_hi = LOG_MAGIC_HI;
		r.magic_lo = LOG_MAGIC_LO;
		r.seq = ++g_log_seq;
		r.type = (u8)type;
		r.len = 20;
		r.tick = Bare_GetTickMs();
		for(i=0;i<20;i++) r.detail[i] = 0;
		if(detail)
		{
				strncpy((char*)r.detail,detail,20);
		}
		r.crc = crc16_ccitt((const u8*)&r.seq,4+1+1+2+4+20);
		r.valid = 0xFFFF; /* 最后再写valid */

		erase_page_if_needed(g_log_wr_slot);

		/* 先写除valid外的内容 */
		STMFLASH_Write(slot_addr(g_log_wr_slot),(u16*)&r,(sizeof(LOG_REC_T)-2+1)/2);
		/* 再写valid，保证掉电不产生“半条有效记录” */
		v = LOG_VALID_FLAG;
		STMFLASH_Write(slot_addr(g_log_wr_slot) + (sizeof(LOG_REC_T)-2),&v,1);

		if(g_log_count < SYSLOG_MAX_SLOTS) g_log_count++;
		g_log_wr_slot = (u16)((g_log_wr_slot + 1) % SYSLOG_MAX_SLOTS);
		return 0;
}

u8 SysLog_ReadByIndex(u16 index,SYSLOG_ITEM_T *item)
{
		LOG_REC_T r;
		u16 slot;
		u16 scanned = 0;
		u16 i;
		u16 want = index;

		if(item==NULL) return 1;
		memset(item,0,sizeof(*item));
		if(g_log_count==0) return 1;

		/* 从“写指针前一个”开始向前找有效记录 */
		slot = (g_log_wr_slot==0) ? (SYSLOG_MAX_SLOTS-1) : (g_log_wr_slot-1);
		while(scanned < SYSLOG_MAX_SLOTS)
		{
				flash_read_rec(slot,&r);
				if(rec_is_valid(&r))
				{
						if(want==0)
						{
								item->seq = r.seq;
								item->type = r.type;
								item->tick = r.tick;
								for(i=0;i<20;i++) item->detail[i] = (char)r.detail[i];
								item->detail[19] = 0;
								return 0;
						}
						want--;
				}
				scanned++;
				slot = (slot==0) ? (SYSLOG_MAX_SLOTS-1) : (slot-1);
		}
		return 1;
}

static u8 match_filter(u8 type,LOG_FILTER_T f)
{
		if(f==LOG_FILTER_ALL) return 1;
		return (type==(u8)f);
}

void SysLog_ReportLast(u8 n,LOG_FILTER_T filter)
{
		u8 i;
		SYSLOG_ITEM_T it;
		char line[64];
		u8 sent=0;

		for(i=0;i<g_log_count && sent<n;i++)
		{
				if(SysLog_ReadByIndex(i,&it)==0)
				{
						if(!match_filter(it.type,filter)) continue;
						sprintf(line,"LOG seq=%lu type=%u tick=%lu %s",(u32)it.seq,(u32)it.type,(u32)it.tick,it.detail);
						uart1_SendStr(line);
						uart1_SendStr("\r\n");
						sent++;
				}
		}
}

void SysLog_OLED_ShowPage(u16 page,LOG_FILTER_T filter)
{
		/* OLED 3行显示(16/32/48)，每页3条 */
		u16 start = page * 3;
		u16 i,idx;
		u16 shown=0;
		SYSLOG_ITEM_T it;
		char line[17];

		OLED_BatchBegin();
		OLED_ShowString(0,16,"LOG:",16);
		OLED_ShowString(0,32,OLED_S16_SP16,16);
		OLED_ShowString(0,48,OLED_S16_SP16,16);

		for(idx=0,i=0; idx<g_log_count && shown<3; idx++)
		{
				if(SysLog_ReadByIndex(idx,&it)!=0) break;
				if(!match_filter(it.type,filter)) continue;
				if(i++ < start) continue;

				/* 16字符一行：T? S????? */
				sprintf(line,"T%u S%lu",(u32)it.type,(u32)it.seq);
				if(shown==0) OLED_ShowString(40,16,line,16);
				else if(shown==1) OLED_ShowString(0,32,line,16);
				else OLED_ShowString(0,48,line,16);
				shown++;
		}
		OLED_BatchEnd();
}

void SysLog_Cmd_Proc(char *cmd)
{
		/* 支持:
		   CMD:LOG=LAST
		   CMD:LOG=LAST,5
		   CMD:LOG=TYPE=ALARM
		   CMD:LOG=TYPE=UNLOCK,5
		*/
		u8 n = 5;
		LOG_FILTER_T f = LOG_FILTER_ALL;
		if(cmd==NULL) return;
		if(strstr(cmd,"LOG=")==NULL) return;

		if(strstr(cmd,"TYPE=ALARM")) f = LOG_FILTER_ALARM;
		else if(strstr(cmd,"TYPE=UNLOCK")) f = LOG_FILTER_UNLOCK;
		else if(strstr(cmd,"TYPE=CONFIG")) f = LOG_FILTER_CONFIG;

		if(sscanf(cmd,"%*[^,],%hhu",&n)==1) {;}

		SysLog_ReportLast(n,f);
}

