#include "sd_spi.h"
#include "spi.h"
#include "delay.h"
#include "board_config.h"

#ifndef SD_SPI_INIT_POWERUP_MS
#define SD_SPI_INIT_POWERUP_MS          12u
#endif
#ifndef SD_SPI_INIT_DUMMY_BYTES
#define SD_SPI_INIT_DUMMY_BYTES         80u
#endif
#ifndef SD_SPI_CMD_WAIT_READY_MS
#define SD_SPI_CMD_WAIT_READY_MS        200u
#endif
#ifndef SD_SPI_SELECT_RETRY
#define SD_SPI_SELECT_RETRY             20000u
#endif
#ifndef SD_SPI_ACMD41_MAX_RETRIES
#define SD_SPI_ACMD41_MAX_RETRIES       600u
#endif
#ifndef SD_SPI_ACMD41_RETRY_DELAY_MS
#define SD_SPI_ACMD41_RETRY_DELAY_MS    2u
#endif
#ifndef SD_SPI_PLACEHOLDER_SECTORS
#define SD_SPI_PLACEHOLDER_SECTORS      8388608u /* 4GiB @ 512B，与 board_config 默认一致 */
#endif
#ifndef SD_SPI_CMD0_MAX_RETRIES
#define SD_SPI_CMD0_MAX_RETRIES          10u
#endif
#ifndef SD_SPI_CMD0_RETRY_DELAY_MS
#define SD_SPI_CMD0_RETRY_DELAY_MS       2u
#endif
#ifndef SD_SPI_WRITE_RETRIES
#define SD_SPI_WRITE_RETRIES             3u
#endif

#define CMD0    (0)
#define CMD8    (8)
#define CMD17   (17)
#define CMD24   (24)
#define CMD55   (55)
#define CMD58   (58)
#define ACMD41  (41)

static u8 sd_type = 0; // 0=unknown 1=SDSC 2=SDHC
static u32 sd_sector_count = 0;
static u8 sd_last_error = 0; /* 0=OK 1=CMD0 2=CMD8 3=ACMD41 4=CMD58 */
static u8 sd_last_cmd0_r1 = 0xFF;
static u8 sd_last_write_stage = 0;
static u8 sd_last_write_r1 = 0xFF;
static u8 sd_last_write_resp = 0xFF;

static u8 spi_txrx(u8 d)
{
		return SPI_WriteByte(SD_SPI,d);
}

static void sd_deselect(void)
{
		SD_CS_H();
		spi_txrx(0xFF);
}

static u8 sd_select(void)
{
		u32 t = SD_SPI_SELECT_RETRY;
		SD_CS_L();
		while(t--)
		{
				if(spi_txrx(0xFF)==0xFF) return 1;
				delay_us(1);
		}
		sd_deselect();
		return 0;
}

static u8 sd_wait_ready(u16 ms)
{
		u16 t;
		for(t=0;t<ms;t++)
		{
				if(spi_txrx(0xFF)==0xFF) return 1;
				delay_ms(1);
		}
		return 0;
}

static u8 sd_send_cmd(u8 cmd,u32 arg,u8 crc)
{
		u8 res;
		u8 n=10;
		if(!sd_select()) return 0xFF;
		if(!sd_wait_ready((u16)SD_SPI_CMD_WAIT_READY_MS)) { sd_deselect(); return 0xFF; }

		spi_txrx(0x40|cmd);
		spi_txrx((u8)(arg>>24));
		spi_txrx((u8)(arg>>16));
		spi_txrx((u8)(arg>>8));
		spi_txrx((u8)arg);
		spi_txrx(crc);

		while(n--)
		{
				res=spi_txrx(0xFF);
				if((res&0x80)==0) break;
		}
		return res;
}

static u8 sd_recv_data(u8 *buf,u16 len)
{
		u16 t=0xFFFF;
		u8 token;
		do{
				token=spi_txrx(0xFF);
		}while(token==0xFF && --t);
		if(token!=0xFE) return 0;
		while(len--) *buf++=spi_txrx(0xFF);
		spi_txrx(0xFF); spi_txrx(0xFF);
		return 1;
}

static u8 sd_send_data(const u8 *buf,u8 token, u8 *resp_out)
{
		u16 i;
		u8 resp;
		u16 t;
		if(resp_out) *resp_out = 0xFF;
		spi_txrx(token);
		for(i=0;i<512;i++) spi_txrx(buf[i]);
		spi_txrx(0xFF); spi_txrx(0xFF);
		/* 数据响应 token 可能延后若干字节，不能只读一次 */
		resp = 0xFF;
		for(t = 0; t < 64u; t++)
		{
				resp = spi_txrx(0xFF);
				if(resp != 0xFF) break;
		}
		if(resp_out) *resp_out = resp;
		if((resp&0x1F)!=0x05) return 0;
		/* 写入后卡会 busy 拉低 MISO，等待其释放 */
		if(!sd_wait_ready(800)) return 0;
		return 1;
}

u8 SD_SPI_Init(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		u8 i,res,ocr[4];
		u16 acmd41_try;
		u16 cmd0_try;

		RCC_APB2PeriphClockCmd(SD_CS_RCC,ENABLE);
		sd_last_error = 0;
		sd_type = 0;
		sd_sector_count = 0;
		sd_last_cmd0_r1 = 0xFF;
		sd_last_write_stage = 0;
		sd_last_write_r1 = 0xFF;
		sd_last_write_resp = 0xFF;
		GPIO_InitStructure.GPIO_Pin = SD_CS_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(SD_CS_PORT,&GPIO_InitStructure);
		SD_CS_H();

		SPI2_Init();
		/* 极低速：CMD0/CMD8/ACMD41 对劣质线材更友好 */
		SPI_SetSpeed(SD_SPI,2);
		delay_ms(SD_SPI_INIT_POWERUP_MS);

		for(i=0;i<SD_SPI_INIT_DUMMY_BYTES;i++) spi_txrx(0xFF);

		/* CMD0 对线材/触点最敏感：加重试与每轮重发空闲时钟 */
		res = 0xFF;
		for(cmd0_try = 0; cmd0_try < SD_SPI_CMD0_MAX_RETRIES; cmd0_try++)
		{
				res = sd_send_cmd(CMD0,0,0x95);
				sd_last_cmd0_r1 = res;
				sd_deselect();
				if(res == 0x01) break;
				for(i = 0; i < SD_SPI_INIT_DUMMY_BYTES; i++) spi_txrx(0xFF);
				delay_ms(SD_SPI_CMD0_RETRY_DELAY_MS);
		}
		if(res!=0x01) { sd_last_error = 1; return 0; }

		/* 进入识别阶段仍保持低速 */
		res = sd_send_cmd(CMD8,0x1AA,0x87);
		if(res==0x01)
		{
				for(i=0;i<4;i++) ocr[i]=spi_txrx(0xFF);
				sd_deselect();
				if(ocr[2]==0x01 && ocr[3]==0xAA)
				{
						acmd41_try = 0;
						do{
								res = sd_send_cmd(CMD55,0,0x01);
								sd_deselect();
								res = sd_send_cmd(ACMD41,0x40000000,0x01);
								sd_deselect();
								if(res == 0)
										break;
								delay_ms(SD_SPI_ACMD41_RETRY_DELAY_MS);
								acmd41_try++;
						}while(acmd41_try < SD_SPI_ACMD41_MAX_RETRIES);
						if(res != 0)
						{
								sd_last_error = 3;
								return 0;
						}

						res = sd_send_cmd(CMD58,0,0x01);
						for(i=0;i<4;i++) ocr[i]=spi_txrx(0xFF);
						sd_deselect();
						if(res != 0x00)
						{
								sd_last_error = 4;
								return 0;
						}
						sd_type = (ocr[0]&0x40)?2:1;
				}
				else
				{
						sd_last_error = 2;
						return 0;
				}
		}
		else
		{
				sd_deselect();
				acmd41_try = 0;
				do{
						res = sd_send_cmd(CMD55,0,0x01);
						sd_deselect();
						res = sd_send_cmd(ACMD41,0,0x01);
						sd_deselect();
						if(res == 0)
								break;
						delay_ms(SD_SPI_ACMD41_RETRY_DELAY_MS);
						acmd41_try++;
				}while(acmd41_try < SD_SPI_ACMD41_MAX_RETRIES);
				if(res != 0)
				{
						sd_last_error = 3;
						return 0;
				}
				sd_type = 1;
		}

		/* 初始化完成后保持中低速：长杜邦线下写块更稳（优先稳定） */
		SPI_SetSpeed(SD_SPI,0);
		sd_sector_count = SD_SPI_PLACEHOLDER_SECTORS;
		return 1;
}

u8 SD_SPI_ReadBlocks(u8 *buf,u32 sector,u32 count)
{
		u8 res;
		if(sd_type!=2) sector <<= 9;
		while(count--)
		{
				res = sd_send_cmd(CMD17,sector,0x01);
				if(res!=0x00){ sd_deselect(); return 0; }
				if(!sd_recv_data(buf,512)){ sd_deselect(); return 0; }
				sd_deselect();
				buf += 512;
				if(sd_type==2) sector++;
				else sector += 512;
		}
		return 1;
}

u8 SD_SPI_WriteBlocks(u8 *buf,u32 sector,u32 count)
{
		u8 res;
		u8 retry;
		u8 data_resp;
		sd_last_write_stage = 0;
		sd_last_write_r1 = 0xFF;
		sd_last_write_resp = 0xFF;
		if(sd_type!=2) sector <<= 9;
		while(count--)
		{
				retry = 0;
				do
				{
						res = sd_send_cmd(CMD24,sector,0x01);
						sd_last_write_r1 = res;
						if(res!=0x00){ sd_last_write_stage = 1; sd_deselect(); retry++; continue; }
						data_resp = 0xFF;
						if(!sd_send_data(buf,0xFE,&data_resp))
						{
								sd_last_write_resp = data_resp;
								sd_last_write_stage = ((data_resp & 0x1Fu) == 0x05u) ? 3 : 2;
								sd_deselect();
								retry++;
								continue;
						}
						sd_deselect();
						break;
				}while(retry < SD_SPI_WRITE_RETRIES);
				if(retry >= SD_SPI_WRITE_RETRIES) return 0;
				buf += 512;
				if(sd_type==2) sector++;
				else sector += 512;
		}
		return 1;
}

u32 SD_SPI_GetSectorCount(void)
{
		return sd_sector_count;
}

u8 SD_SPI_GetType(void)
{
		return sd_type;
}

u8 SD_SPI_GetLastError(void)
{
		return sd_last_error;
}

u8 SD_SPI_GetLastCmd0R1(void)
{
		return sd_last_cmd0_r1;
}

void SD_SPI_DebugGetPinLevels(u8 *cs, u8 *sck, u8 *miso, u8 *mosi)
{
		/* SPI2: CS=PB12, SCK=PB13, MISO=PB14, MOSI=PB15 */
		if(cs)   *cs   = (u8)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);
		if(sck)  *sck  = (u8)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13);
		if(miso) *miso = (u8)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14);
		if(mosi) *mosi = (u8)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15);
}

u8 SD_SPI_GetLastWriteStage(void)
{
		return sd_last_write_stage;
}

u8 SD_SPI_GetLastWriteR1(void)
{
		return sd_last_write_r1;
}

u8 SD_SPI_GetLastWriteResp(void)
{
		return sd_last_write_resp;
}

