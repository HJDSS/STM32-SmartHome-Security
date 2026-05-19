/*
 * SD 容量统计（占位版）
 *
 * 说明：当前工程编译环境中找不到 FatFs 的 ff.h（按你的要求）。
 * 为保证工程可编译通过，本文件先提供不依赖 FatFs 的“编译占位实现”：
 * - SD 统计始终返回 unmounted/0
 * - JSON 格式与 APP 保持一致
 * 后续你确认 ff.h include 路径正确后，我再把这里替换成真实 f_getfree 统计实现。
 */

#include "sd_capacity.h"
#include <stdio.h>
#include <string.h>
#include "board_config.h"
#include "stm32f10x_gpio.h"

extern u8 dht11_temp;
extern u8 dht11_humi;
extern u8 dht11_data_valid;
extern u16 mq2_adc_value;
extern u8 security_mode;

/* 门锁继电器 BOARD_DOOR_LOCK（默认 PC6）：低电平为开锁（与 main 中 RELAY=0 一致） */
static const char *sd_door_string(void)
{
	if(GPIO_ReadOutputDataBit(BOARD_DOOR_LOCK_PORT, BOARD_DOOR_LOCK_PIN) == Bit_RESET)
		return "open";
	return "closed";
}

/* SD 初始化（占位，不依赖 FatFs） */
void SdCapacity_Init(void)
{
	/* 不做任何事：当前为编译占位版 */
}

/* 获取容量信息（占位：始终 unmounted） */
void SdCapacity_FetchInfo(SD_CAP_INFO_T *info)
{
	if(info == NULL) return;
	info->mounted = 0;
	info->total_gb = 0.f;
	info->used_gb = 0.f;
	info->usage_pct = 0;
}

/* 输出 SD 子对象 JSON（占位：始终 unmounted） */
void SdCapacity_PrintJsonReport(const SD_CAP_INFO_T *info)
{
	(void)info;
	printf("{\"sd\":{\"status\":\"unmounted\",\"total\":0.0,\"used\":0.0,\"usage\":0}}\r\n");
}

/* 合并温湿度/燃气/门锁/布防与 SD 的整包 JSON（按 APP 格式） */
int DeviceStatus_BuildMqttJson(char *buf, unsigned buf_size)
{
	SD_CAP_INFO_T sd;
	float temp_f;
	float humi_f;
	unsigned gas;
	const char *door;
	int arm;
	const char *sst;
	int n;

	if(buf == NULL || buf_size < 32u) return -1;

	SdCapacity_FetchInfo(&sd);

	if(dht11_data_valid)
	{
		temp_f = (float)dht11_temp;
		humi_f = (float)dht11_humi;
	}
	else
	{
		temp_f = 0.f;
		humi_f = 0.f;
	}

	gas = (unsigned)mq2_adc_value;
	door = sd_door_string();
	arm = ((int)security_mode != 0) ? 1 : 0;
	sst = sd.mounted ? "mounted" : "unmounted";

	n = snprintf((char *)buf, buf_size,
			"{\"temp\":%.1f,\"humi\":%.1f,\"gas\":%u,\"door\":\"%s\",\"arm\":%d,\"sd\":{\"status\":\"%s\",\"total\":0.0,\"used\":0.0,\"usage\":0}}",
			temp_f, humi_f, gas, door, arm, sst);
	if(n <= 0 || (unsigned)n >= buf_size) return -1;
	return n;
}

void DeviceStatus_PrintMqttJson(void)
{
	char line[320];
	int n = DeviceStatus_BuildMqttJson(line, sizeof(line));
	if(n > 0) printf("%s\r\n", line);
}

/* OneNET 物模型：标识符需与控制台物模型定义一致，可按需改名 */
int DeviceStatus_BuildOneNETThingPropertyJson(char *buf, unsigned buf_size)
{
	static unsigned prop_id = 1;
	SD_CAP_INFO_T sd;
	float temp_f;
	float humi_f;
	unsigned gas;
	const char *door;
	int arm;
	int n;

	if(buf == NULL || buf_size < 64u) return -1;

	SdCapacity_FetchInfo(&sd);

	if(dht11_data_valid)
	{
		temp_f = (float)dht11_temp;
		humi_f = (float)dht11_humi;
	}
	else
	{
		temp_f = 0.f;
		humi_f = 0.f;
	}

	gas = (unsigned)mq2_adc_value;
	door = sd_door_string();
	arm = ((int)security_mode != 0) ? 1 : 0;

	n = snprintf((char *)buf, buf_size,
			"{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
			"\"temp\":{\"value\":%.1f},\"humi\":{\"value\":%.1f},\"gas\":{\"value\":%u},"
			"\"door\":{\"value\":\"%s\"},\"arm\":{\"value\":%d}"
			"}}",
			(unsigned)(prop_id++), temp_f, humi_f, gas, door, arm);
	if(n <= 0 || (unsigned)n >= buf_size) return -1;
	return n;
}
