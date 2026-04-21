#include "diskio.h"
#include "sd_spi.h"

DSTATUS disk_initialize (BYTE pdrv)
{
		u8 i;
		(void)pdrv;
		for(i = 0; i < 3u; i++)
		{
				if(SD_SPI_Init()) return 0;
		}
		return STA_NOINIT;
}

DSTATUS disk_status (BYTE pdrv)
{
		(void)pdrv;
		return 0;
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
		(void)pdrv;
		if(SD_SPI_ReadBlocks(buff,(u32)sector,count)) return RES_OK;
		return RES_ERROR;
}

#if FF_FS_READONLY == 0
DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
		(void)pdrv;
		if(SD_SPI_WriteBlocks((u8*)buff,(u32)sector,count)) return RES_OK;
		return RES_ERROR;
}
#endif

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{
		(void)pdrv;
		switch(cmd)
		{
				case CTRL_SYNC:
						return RES_OK;
				case GET_SECTOR_COUNT:
						*(DWORD*)buff = SD_SPI_GetSectorCount();
						return RES_OK;
				case GET_SECTOR_SIZE:
						*(WORD*)buff = 512;
						return RES_OK;
				case GET_BLOCK_SIZE:
						*(DWORD*)buff = 1;
						return RES_OK;
				default:
						return RES_PARERR;
		}
}

DWORD get_fattime (void)
{
		return 0;
}

