#ifndef _SPI_H_
#define _SPI_H_

#include "sys.h"
#include "stm32f10x_spi.h"

u8 SPI_WriteByte(SPI_TypeDef* SPIx,u8 Byte);
void SPI2_Init(void);
void SPI_SetSpeed(SPI_TypeDef* SPIx,u8 SpeedSet);

#endif

