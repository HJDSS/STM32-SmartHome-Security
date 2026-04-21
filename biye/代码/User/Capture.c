#include <stdio.h>
#include <string.h>

#include "board_config.h"
#include "Capture.h"
#include "capture_task.h"

#include "ov7670_fifo.h"
#include "spi.h"
#include "sd_spi.h"
#include "../Middlewares/FatFs/ff.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_exti.h"
#include "misc.h"

#include "delay.h"
#include "linkage.h"

/* 兼容需求中函数名 */
#define OV7670_StartCapture()     OV7670_FIFO_StartCapture()
#define OV7670_ResetReadPtr()     OV7670_FIFO_ResetReadPtr()
#define OV7670_ReadByte()         OV7670_FIFO_ReadByte()

#define CAP_W                 320u
#define CAP_H                 240u
#define CAP_ROW_BYTES         (CAP_W * 2u)
#define CAP_PIXEL_BYTES       (CAP_W * CAP_H * 2u)
#define BMP_HDR_BYTES         (54u + 12u)
#define BMP_PIXEL_OFFSET      (BMP_HDR_BYTES)
#define BMP_FILE_SIZE         (BMP_HDR_BYTES + CAP_PIXEL_BYTES)

volatile u8 g_capture_busy = 0;

#if EN_OV7670_LOCAL
static FATFS s_fatfs;
static u8 s_fat_mounted;

/* 本地 OV7670 抓拍；键盘 Y3/Y4 固定 PB1/PB2（与 PA0~PA7 数据口无共脚）。 */
static void write_bmp_header_rgb565_bottom_up(FIL *fp)
{
	UINT bw;
	u8 hdr[54] = {0};
	u32 file_size = (u32)BMP_FILE_SIZE;
	u32 offbits = (u32)BMP_PIXEL_OFFSET;
	u32 dib = 40;
	s32 w = (s32)CAP_W;
	s32 h = (s32)CAP_H; /* 正高度：BMP 底向上存储 */
	u16 planes = 1, bpp = 16;
	u32 comp = 3; /* BI_BITFIELDS */
	u32 imgsize = (u32)CAP_PIXEL_BYTES;
	u32 rmask = 0xF800, gmask = 0x07E0, bmask = 0x001F;

	hdr[0] = 'B';
	hdr[1] = 'M';
	hdr[2] = (u8)file_size;
	hdr[3] = (u8)(file_size >> 8);
	hdr[4] = (u8)(file_size >> 16);
	hdr[5] = (u8)(file_size >> 24);
	hdr[10] = (u8)offbits;
	hdr[14] = (u8)dib;
	hdr[18] = (u8)w;
	hdr[19] = (u8)(w >> 8);
	hdr[20] = (u8)(w >> 16);
	hdr[21] = (u8)(w >> 24);
	hdr[22] = (u8)h;
	hdr[23] = (u8)(h >> 8);
	hdr[24] = (u8)(h >> 16);
	hdr[25] = (u8)(h >> 24);
	hdr[26] = (u8)planes;
	hdr[28] = (u8)bpp;
	hdr[30] = (u8)comp;
	hdr[34] = (u8)imgsize;
	hdr[35] = (u8)(imgsize >> 8);
	hdr[36] = (u8)(imgsize >> 16);
	hdr[37] = (u8)(imgsize >> 24);

	(void)f_write(fp, hdr, 54, &bw);
	(void)f_write(fp, &rmask, 4, &bw);
	(void)f_write(fp, &gmask, 4, &bw);
	(void)f_write(fp, &bmask, 4, &bw);
}
#endif

#if EN_OV7670_LOCAL
static void capture_one_to_sd(void)
{
	FRESULT fr;
	FIL fp;
	char name[32];
	u32 tick = Bare_GetTickMs();
	u16 y;
	u8 row[CAP_ROW_BYTES];
	UINT bw;

	snprintf(name, sizeof(name), "0:CAP_%lu.BMP", (unsigned long)tick);

	if(!s_fat_mounted)
	{
		if(f_mount(&s_fatfs, "0:", 1) == FR_OK)
			s_fat_mounted = 1;
		else
			return;
	}

	fr = f_open(&fp, name, FA_CREATE_ALWAYS | FA_WRITE);
	if(fr != FR_OK)
		return;

	write_bmp_header_rgb565_bottom_up(&fp);

	/* 采集：StartCapture 内部等待 VSYNC 边沿，视为采集完成标志 */
	OV7670_StartCapture();
	OV7670_ResetReadPtr();

	/* 倒序写入：先读到 rowbuf，再写到 BMP 的 (CAP_H-1-y) 行 */
	for(y = 0; y < CAP_H; y++)
	{
		u16 i;
		FSIZE_t off;

		for(i = 0; i < CAP_ROW_BYTES; i++)
			row[i] = OV7670_ReadByte();

		off = (FSIZE_t)BMP_PIXEL_OFFSET + (FSIZE_t)(CAP_H - 1u - y) * (FSIZE_t)CAP_ROW_BYTES;
		(void)f_lseek(&fp, off);
		fr = f_write(&fp, row, CAP_ROW_BYTES, &bw);
		if(fr != FR_OK || bw != CAP_ROW_BYTES)
			break;
	}

	(void)f_close(&fp);

	if(fr == FR_OK)
		Capture_OnSaved(name);
}
#endif

__weak void Capture_OnSaved(const char *filename)
{
	/* 默认回调：仅做联动上报（可在别处提供同名强符号覆盖） */
	char msg[64];
	if(filename == NULL) return;
	sprintf(msg, "CAPTURE:SAVED:%s", filename);
	Linkage_MQTT_Report(msg);
}

void Bare_CapturePoll(void)
{
#if EN_OV7670_LOCAL
	if(g_cap_evt_pending == 0u)
		return;
	if(g_capture_busy)
		return;
	g_cap_evt_pending = 0u;
	capture_one_to_sd();
	delay_ms(50);
#else
	(void)0;
#endif
}

