#ifndef __SD_SPI_H
#define __SD_SPI_H

#include "sys.h"

// SD SPI硬件配置(按接线修改)
#define SD_SPI                SPI2
#define SD_SPI_RCC            RCC_APB1Periph_SPI2

#include "board_config.h"

#define SD_CS_PORT            BOARD_SD_CS_PORT
#define SD_CS_PIN             BOARD_SD_CS_PIN
#define SD_CS_RCC             BOARD_SD_CS_CLK

#define SD_CS_L()             GPIO_ResetBits(SD_CS_PORT,SD_CS_PIN)
#define SD_CS_H()             GPIO_SetBits(SD_CS_PORT,SD_CS_PIN)

u8 SD_SPI_Init(void);
u8 SD_SPI_ReadBlocks(u8 *buf,u32 sector,u32 count);
u8 SD_SPI_WriteBlocks(u8 *buf,u32 sector,u32 count);
u32 SD_SPI_GetSectorCount(void);
u8 SD_SPI_GetType(void); /* 0=UNK 1=SDSC 2=SDHC */
u8 SD_SPI_GetLastError(void); /* 0=OK 1=CMD0 2=CMD8 3=ACMD41 4=CMD58 */
u8 SD_SPI_GetLastCmd0R1(void);
void SD_SPI_DebugGetPinLevels(u8 *cs, u8 *sck, u8 *miso, u8 *mosi);
u8 SD_SPI_GetLastWriteStage(void); /* 0=ok 1=CMD24_R1 2=DATA_RESP 3=BUSY_TIMEOUT */
u8 SD_SPI_GetLastWriteR1(void);
u8 SD_SPI_GetLastWriteResp(void);

#endif

