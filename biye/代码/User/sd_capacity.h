#ifndef __SD_CAPACITY_H
#define __SD_CAPACITY_H

#include "sys.h"

/*
 * SD 容量与挂载状态。
 * total_gb / used_gb：SI 千兆字节（1GB = 1e9 字节），供 JSON 使用 %.1f。
 */
typedef struct
{
		u8 mounted;     /* 1：已挂载且 f_getfree 成功；0：未挂载或失败 */
		float total_gb; /* 总容量（GB，约一位小数精度） */
		float used_gb;  /* 已用容量（GB） */
		int usage_pct;  /* 使用率 0～100 */
} SD_CAP_INFO_T;

/* 挂载逻辑盘 "0:"，建议在 USART1 初始化之后调用 */
void SdCapacity_Init(void);

/* 调用 f_getfree 填充 info；失败时 mounted=0，容量为 0，不崩溃 */
void SdCapacity_FetchInfo(SD_CAP_INFO_T *info);

/* 仅 sd 子对象 JSON（printf → USART1） */
void SdCapacity_PrintJsonReport(const SD_CAP_INFO_T *info);

/* 温湿度、燃气、门锁、布防与 SD 合并 JSON（printf → USART1） */
void DeviceStatus_PrintMqttJson(void);

/* 写入 buf，成功返回长度（不含 \\0），失败返回 -1 */
int DeviceStatus_BuildMqttJson(char *buf, unsigned buf_size);

/* OneNET 物模型属性上报 JSON（id/version/params/...） */
int DeviceStatus_BuildOneNETThingPropertyJson(char *buf, unsigned buf_size);

#endif
