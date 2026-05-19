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
#include "esp8266_tls.h"
#include "esp8266_onenet_mqtt.h"
#include "syslog.h"
#include "app_params.h"
#include "log.h"
#include "oled_view.h"

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

#if BOARD_IWDG_ENABLE
static void cap_iwdg_kick(void)
{
    IWDG->KR = (u16)0xAAAA;
}
#else
static void cap_iwdg_kick(void) {}
#endif

#if EN_OV7670_LOCAL
static FATFS s_fatfs;
static u8 s_fat_mounted;
volatile u16 g_cap_replay_count = 0u;
volatile u16 g_cap_sd_write_fail_count = 0u;
volatile u8  g_cap_last_ok = 0u;            /* 🟡12: 最近一次抓拍是否成功 */
volatile u16 g_cap_validation_fail_count = 0u; /* 🟡12: BMP文件大小校验失败计数 */
extern volatile u16 g_cap_total_attempts;
extern volatile u16 g_cap_success_count;
#define CAP_OFFLINE_Q_DEPTH  ((u8)APP_CAP_OFFLINE_Q_DEPTH)
typedef struct
{
    u8 used;
    u32 tick;
    char name[32];
} cap_offline_item_t;
static cap_offline_item_t s_cap_q[CAP_OFFLINE_Q_DEPTH];
static u8 s_cap_q_head = 0u;
static u8 s_cap_q_tail = 0u;
volatile u16 g_cap_q_drop = 0u;
volatile u16 g_cap_q_flush_ok = 0u;
volatile u16 g_cap_q_flush_fail = 0u;

static u8 s_cap_row_wr[CAP_ROW_BYTES];

static void cap_read_fifo_row_rgb565(u8 *row)
{
    u16 i;
    if(row == NULL) return;
    for(i = 0; i < CAP_ROW_BYTES; i++)
        row[i] = OV7670_FIFO_ReadByte();
}

static u8 capture_publish_saved(const char *filename, u32 tick)
{
    char topic[120];
    char payload[192];
    if(filename == NULL) return 0u;
    if(ESP8266_Online_Flag == 0u) return 0u;
    sprintf(topic, "$sys/%s/%s/thing/property/post", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    sprintf(payload,
            "{\"id\":\"cap%lu\",\"version\":\"1.0\",\"params\":{\"capture_path\":{\"value\":\"%s\"},\"capture_ts\":{\"value\":%lu}}}",
            (unsigned long)tick, filename, (unsigned long)tick);
    return OneNET_AT_Mqtt_PublishRaw(topic, payload, (u16)strlen(payload));
}

static void capture_enqueue_saved(const char *filename, u32 tick)
{
    u8 next;
    if(filename == NULL) return;
    next = (u8)((s_cap_q_tail + 1u) % CAP_OFFLINE_Q_DEPTH);
    if(next == s_cap_q_head && s_cap_q[s_cap_q_head].used)
    {
        s_cap_q_head = (u8)((s_cap_q_head + 1u) % CAP_OFFLINE_Q_DEPTH);
        g_cap_q_drop++;
        SysLog_Add(LOG_EVT_ALARM, "CAP_Q_DROP");
    }
    s_cap_q[s_cap_q_tail].used = 1u;
    s_cap_q[s_cap_q_tail].tick = tick;
    strncpy(s_cap_q[s_cap_q_tail].name, filename, sizeof(s_cap_q[s_cap_q_tail].name) - 1u);
    s_cap_q[s_cap_q_tail].name[sizeof(s_cap_q[s_cap_q_tail].name) - 1u] = '\0';
    s_cap_q_tail = next;
}

static void capture_flush_offline_queue(void)
{
    while(s_cap_q_head != s_cap_q_tail && s_cap_q[s_cap_q_head].used)
    {
        char sent_name[32];
        if(ESP8266_Online_Flag == 0u) break;
        strncpy(sent_name, s_cap_q[s_cap_q_head].name, sizeof(sent_name) - 1u);
        sent_name[sizeof(sent_name) - 1u] = '\0';
        if(capture_publish_saved(s_cap_q[s_cap_q_head].name, s_cap_q[s_cap_q_head].tick))
        {
            s_cap_q[s_cap_q_head].used = 0u;
            s_cap_q_head = (u8)((s_cap_q_head + 1u) % CAP_OFFLINE_Q_DEPTH);
            g_cap_q_flush_ok++;
            g_cap_replay_count++;
            SysLog_Add(LOG_EVT_CONFIG, "CAP_Q_FLUSH_OK");
            LOG_NET("cap replay ok: %s", sent_name);
        }
        else
        {
            g_cap_q_flush_fail++;
            SysLog_Add(LOG_EVT_ALARM, "CAP_Q_FLUSH_FAIL");
            LOG_NET("cap replay fail");
            break;
        }
    }
}

static void write_bmp_header_rgb565_topdown(FIL *fp)
{
    UINT bw;
    u8 hdr[54] = {0};
    u32 file_size = (u32)BMP_FILE_SIZE;
    u32 offbits = (u32)BMP_PIXEL_OFFSET;
    u32 dib = 40;
    s32 w = (s32)CAP_W;
    s32 h = -(s32)CAP_H;
    u16 planes = 1, bpp = 16;
    u32 comp = 3;
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
    hdr[19] = (u8)((u32)w >> 8);
    hdr[20] = (u8)((u32)w >> 16);
    hdr[21] = (u8)((u32)w >> 24);
    hdr[22] = (u8)((u32)h & 0xFFu);
    hdr[23] = (u8)(((u32)h >> 8) & 0xFFu);
    hdr[24] = (u8)(((u32)h >> 16) & 0xFFu);
    hdr[25] = (u8)(((u32)h >> 24) & 0xFFu);
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

static void capture_one_to_sd(void)
{
    FRESULT fr;
    FIL fp;
    char name[32];
    u32 tick = Bare_GetTickMs();
    u16 y;
    UINT bw;
    FILINFO fno;
    u8 ok;

    g_cap_total_attempts++;

    snprintf(name, sizeof(name), "0:CAP_%lu.BMP", (unsigned long)tick);

    if(!s_fat_mounted)
    {
        if(f_mount(&s_fatfs, "0:", 1) == FR_OK)
        {
            s_fat_mounted = 1;
            SysLog_Add(LOG_EVT_CONFIG, "SD_MOUNT_OK");
            LOG_SD("mount ok");
        }
        else
        {
            SysLog_Add(LOG_EVT_ALARM, "SD_MOUNT_FAIL");
            LOG_SD("mount fail");
            return;
        }
    }

    fr = f_open(&fp, name, FA_CREATE_ALWAYS | FA_WRITE);
    if(fr != FR_OK)
        return;

    write_bmp_header_rgb565_topdown(&fp);

    SysLog_Add(LOG_EVT_CONFIG, "CAP_BEGIN");
    LOG_SD("capture begin");
    if(!OV7670_FIFO_StartCaptureTimeout(800u))
    {
        SysLog_Add(LOG_EVT_ALARM, "CAP_VSYNC_TO");
        (void)f_close(&fp);
        (void)f_unlink(name);
        return;
    }
    OV7670_ResetReadPtr();

    SPI_SetSpeed(SD_SPI, 1);  /* 切到 18MHz 写像素数据 */

    ok = 1u;
    for(y = 0; y < CAP_H; y++)
    {
        cap_read_fifo_row_rgb565(s_cap_row_wr);
        fr = f_write(&fp, s_cap_row_wr, CAP_ROW_BYTES, &bw);
        if(fr != FR_OK || bw != CAP_ROW_BYTES)
        {
            ok = 0u;
            break;
        }
        if((y & 0x07u) == 0u)
            cap_iwdg_kick();
    }

    SPI_SetSpeed(SD_SPI, 0);  /* 切回 1.125MHz 安全速率 */
    (void)f_close(&fp);

    if(ok && f_stat(name, &fno) == FR_OK && (FSIZE_t)fno.fsize == (FSIZE_t)BMP_FILE_SIZE)
    {
        g_cap_success_count++;
        g_cap_last_ok = 1u;
        SysLog_Add(LOG_EVT_CONFIG, "CAP_END_OK");
        LOG_SD("bmp save ok");
        Capture_OnSaved(name);
    }
    else
    {
        if(!ok) { g_cap_sd_write_fail_count++; }
        else { g_cap_validation_fail_count++; (void)f_unlink(name); }
        g_cap_last_ok = 0u;
        SysLog_Add(LOG_EVT_ALARM, "CAP_END_FAIL");
        LOG_SD("bmp save fail");
    }
}

u8 Capture_LocalSnapshot(void)
{
    FRESULT fr;
    FIL fp;
    char name[32];
    u32 tick = Bare_GetTickMs();
    u16 y;
    UINT bw;
    FRESULT close_fr;
    FILINFO fno;
    u8 ok = 0u;

    g_cap_total_attempts++;

    if(g_capture_busy)
    {
        OLED_View_ToastRowAB("CAP BUSY        ", 1200u);
        return 0u;
    }
    g_capture_busy = 1u;

#if BOARD_CAP_LOCAL_FIXED_NAME_ENABLE
    strncpy(name, BOARD_CAP_LOCAL_FIXED_BMP_NAME, sizeof(name) - 1u);
    name[sizeof(name) - 1u] = '\0';
#else
    snprintf(name, sizeof(name), "0:CAP_%06lu.BMP", (unsigned long)(tick % 1000000u));
#endif

    if(!s_fat_mounted)
    {
        if(f_mount(&s_fatfs, "0:", 1) != FR_OK)
        {
            SysLog_Add(LOG_EVT_ALARM, "CAP_L_MNT_FAIL");
            OLED_View_ToastRowAB("CAP MNT FAIL    ", 2000u);
            g_capture_busy = 0u;
            return 0u;
        }
        s_fat_mounted = 1u;
    }

    fr = f_open(&fp, name, FA_CREATE_ALWAYS | FA_WRITE);
    if(fr != FR_OK)
    {
        char em[20];
        snprintf(em, sizeof(em), "CAP_L_OP_%u", (unsigned)fr);
        SysLog_Add(LOG_EVT_ALARM, em);
        OLED_View_ToastRowAB("CAP OPEN FAIL   ", 2000u);
        g_capture_busy = 0u;
        return 0u;
    }

    write_bmp_header_rgb565_topdown(&fp);
    cap_iwdg_kick();
    SysLog_Add(LOG_EVT_CONFIG, "CAP_BEGIN");
    LOG_SD("cap local begin");

#if (BOARD_OV7670_FRAME_GAP_MS > 0u)
    delay_ms((u16)BOARD_OV7670_FRAME_GAP_MS);
#endif

    if(!OV7670_FIFO_StartCaptureTimeout(800u))
    {
        (void)f_close(&fp);
        (void)f_unlink(name);
        SysLog_Add(LOG_EVT_ALARM, "CAP_L_VSYNC_TO");
        OLED_View_ToastRowAB("CAP VSYNC TIMO  ", 2000u);
        g_capture_busy = 0u;
        return 0u;
    }
    OV7670_FIFO_ResetReadPtr();

    SPI_SetSpeed(SD_SPI, 1);  /* 切到 18MHz 写像素数据 */

    ok = 1u;
    for(y = 0; y < CAP_H; y++)
    {
        cap_read_fifo_row_rgb565(s_cap_row_wr);
        fr = f_write(&fp, s_cap_row_wr, CAP_ROW_BYTES, &bw);
        if(fr != FR_OK || bw != CAP_ROW_BYTES)
        {
            ok = 0u;
            break;
        }
        if((y & 0x07u) == 0u)
            cap_iwdg_kick();
    }

    SPI_SetSpeed(SD_SPI, 0);  /* 切回 1.125MHz 安全速率 */

    if(!ok)
    {
        (void)f_close(&fp);
        (void)f_unlink(name);
        g_cap_sd_write_fail_count++;
        g_cap_last_ok = 0u;  /* 🟡12 */
        SysLog_Add(LOG_EVT_ALARM, "CAP_SD_WRITE_FAIL");
        OLED_View_ToastRowAB("CAP WRITE FAIL  ", 2000u);
        g_capture_busy = 0u;
        return 0u;
    }

    (void)f_sync(&fp);
    close_fr = f_close(&fp);
    if(close_fr != FR_OK)
    {
        char em[20];
        snprintf(em, sizeof(em), "CAP_L_CL_%u", (unsigned)close_fr);
        SysLog_Add(LOG_EVT_ALARM, em);
        ok = 0u;
        (void)f_unlink(name);
    }
    else if(f_stat(name, &fno) != FR_OK || (FSIZE_t)fno.fsize != (FSIZE_t)BMP_FILE_SIZE)
    {
        g_cap_validation_fail_count++;  /* 🟡12: 校验失败独立计数 */
        SysLog_Add(LOG_EVT_ALARM, "CAP_L_SIZE_BAD");
        (void)f_unlink(name);
        ok = 0u;
    }

    if(!ok)
    {
        g_cap_sd_write_fail_count++;
        g_cap_last_ok = 0u;  /* 🟡12 */
        OLED_View_ToastRowAB("CAP SAVE FAIL   ", 2000u);
        g_capture_busy = 0u;
        return 0u;
    }

    g_cap_success_count++;
    g_cap_last_ok = 1u;  /* 🟡12 */
    SysLog_Add(LOG_EVT_CONFIG, "CAP_END_OK");
    Capture_OnSaved(name);
    g_capture_busy = 0u;
    return 1u;
}
#endif

#if !EN_OV7670_LOCAL
u8 Capture_LocalSnapshot(void)
{
    return 0u;
}
#endif

__weak void Capture_OnSaved(const char *filename)
{
    char msg[64];
    if(filename == NULL) return;
    sprintf(msg, "CAPTURE:SAVED:%s", filename);
    Linkage_MQTT_Report(msg);
#if EN_OV7670_LOCAL
    {
        u32 tick = Bare_GetTickMs();
    if(capture_publish_saved(filename, tick))
    {
        SysLog_Add(LOG_EVT_CONFIG, "CAP_UP_OK");
    }
    else
    {
        capture_enqueue_saved(filename, tick);
        SysLog_Add(LOG_EVT_ALARM, "CAP_UP_DEFER");
    }
    }
#endif
}

void Bare_CapturePoll(void)
{
#if EN_OV7670_LOCAL
    if(g_cap_evt_pending == 0u)
    {
        capture_flush_offline_queue();
        return;
    }
    if(g_capture_busy)
        return;
    g_cap_evt_pending = 0u;
    capture_one_to_sd();
    capture_flush_offline_queue();
    delay_ms(50);
#else
    (void)0;
#endif
}
