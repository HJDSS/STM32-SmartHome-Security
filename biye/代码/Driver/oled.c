#include "oled.h"
#include "stdlib.h"
#include "oledfont.h"  	 
#include "delay.h"
#include "stm32f10x.h"
#include "sys.h"

/* 128x64: 8 pages x 128 bytes */
u8 OLED_GRAM[128][8];
/* 刷屏时按行拷入，一次 I2C burst 发送，避免栈上 128B */
static u8 s_oled_refresh_line[128];
/* >0 时 ShowString/汉字末尾不立即 OLED_Refresh，由 OLED_BatchEnd 统一刷 */
static u8 g_oled_batch_depth;

/* SSD1306 I2C write addr 0x78 (7-bit 0x3C) or 0x7A (0x3D) */
static u8 g_oled_i2c_waddr = OLED_I2C_ADDR;
/* OLED_Init ??????? ACK??????????? OLED_Probe()???????? */
static u8 g_oled_probe_ok = 0;

/* OLED_I2C_BIT_DELAY_US：在 board_config.h 中定义，默认 3μs */

void Send_Byte(u8 dat);

/* If slave holds SDA low: pulse SCL to release bus */
static void oled_i2c_bus_recovery(void)
{
	u8 i;
	OLED_SDIN_Set();
	for (i = 0; i < 9u; i++) {
		OLED_SCLK_Clr();
		delay_us(5);
		OLED_SCLK_Set();
		delay_us(5);
	}
	OLED_SCLK_Clr();
	delay_us(5);
}

static u8 oled_i2c_probe_raw(u8 waddr)
{
	u8 ok;
	I2C_Start();
	Send_Byte(waddr);
	ok = I2C_WaitAck() ? 1 : 0;
	I2C_Stop();
	return ok;
}

//???????
void OLED_ColorTurn(u8 i)
{
	if(i==0)
		{
			OLED_WR_Byte(0xA6,OLED_CMD);//???????
		}
	if(i==1)
		{
			OLED_WR_Byte(0xA7,OLED_CMD);//??????
		}
}

//??????180??
void OLED_DisplayTurn(u8 i)
{
	if(i==0)
		{
			OLED_WR_Byte(0xC8,OLED_CMD);//???????
			OLED_WR_Byte(0xA1,OLED_CMD);
		}
	if(i==1)
		{
			OLED_WR_Byte(0xC0,OLED_CMD);//??????
			OLED_WR_Byte(0xA0,OLED_CMD);
		}
}

//??????
void I2C_Start(void)
{
	OLED_SDIN_Set();
	OLED_SCLK_Set();
	delay_us(OLED_I2C_BIT_DELAY_US);
	OLED_SDIN_Clr();
	delay_us(OLED_I2C_BIT_DELAY_US);
	OLED_SCLK_Clr();
	delay_us(OLED_I2C_BIT_DELAY_US);
}

//???????
void I2C_Stop(void)
{
	/* 标准 I2C Stop：SCL 低时先拉低 SDA，再拉高 SCL，最后释放 SDA */
	OLED_SCLK_Clr();
	delay_us(OLED_I2C_BIT_DELAY_US);
	OLED_SDIN_Clr();
	delay_us(OLED_I2C_BIT_DELAY_US);
	OLED_SCLK_Set();
	delay_us(OLED_I2C_BIT_DELAY_US);
	OLED_SDIN_Set();
	delay_us(OLED_I2C_BIT_DELAY_US);
}

//?????????
u8 I2C_WaitAck(void) //1:ACK 0:NACK
{
	GPIO_InitTypeDef GPIO_InitStructure;
	u32 t = 0;

	GPIO_InitStructure.GPIO_Pin = BOARD_OLED_PIN_SDA;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_OLED_GPIO_PORT, &GPIO_InitStructure);
	GPIO_SetBits(BOARD_OLED_GPIO_PORT, BOARD_OLED_PIN_SDA);

	delay_us(OLED_I2C_BIT_DELAY_US);
	OLED_SCLK_Set();
	delay_us(OLED_I2C_BIT_DELAY_US);
	while(GPIO_ReadInputDataBit(BOARD_OLED_GPIO_PORT, BOARD_OLED_PIN_SDA))
	{
		if(t++ > 8000u) break;
	}
	OLED_SCLK_Clr();
	delay_us(OLED_I2C_BIT_DELAY_US);

	GPIO_InitStructure.GPIO_Pin = BOARD_OLED_PIN_SDA;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_OLED_GPIO_PORT, &GPIO_InitStructure);

	return (t <= 8000u) ? 1 : 0;
}

/* Try 0x78 / 0x7A and update g_oled_i2c_waddr */
u8 OLED_Probe(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(BOARD_OLED_GPIO_CLK, ENABLE);
	GPIO_InitStructure.GPIO_Pin = BOARD_OLED_PIN_SCL | BOARD_OLED_PIN_SDA;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BOARD_OLED_GPIO_PORT, &GPIO_InitStructure);
	GPIO_SetBits(BOARD_OLED_GPIO_PORT, BOARD_OLED_PIN_SCL | BOARD_OLED_PIN_SDA);
	delay_ms(5);
	oled_i2c_bus_recovery();

	if (oled_i2c_probe_raw(0x78)) {
		g_oled_i2c_waddr = 0x78;
		return 1;
	}
	if (oled_i2c_probe_raw(0x7A)) {
		g_oled_i2c_waddr = 0x7A;
		return 1;
	}
	return 0;
}

u8 OLED_GetActiveI2cAddr(void)
{
	return g_oled_i2c_waddr;
}

u8 OLED_GetProbeOk(void)
{
	return g_oled_probe_ok;
}

//?????????
void Send_Byte(u8 dat)
{
	u8 i;
	for(i=0;i<8;i++)
	{
		OLED_SCLK_Clr();
		delay_us(OLED_I2C_BIT_DELAY_US);
		if(dat&0x80)
			OLED_SDIN_Set();
		else
			OLED_SDIN_Clr();
		delay_us(OLED_I2C_BIT_DELAY_US);
		OLED_SCLK_Set();
		delay_us(OLED_I2C_BIT_DELAY_US);
		OLED_SCLK_Clr();
		delay_us(OLED_I2C_BIT_DELAY_US);
		dat<<=1;
	}
}

//??????????
//??SSD1306??????????
//mode:????/???????0,???????;1,???????;
void OLED_BatchBegin(void)
{
	if(g_oled_batch_depth < 250u)
		g_oled_batch_depth++;
}

void OLED_BatchEnd(void)
{
	if(g_oled_batch_depth)
		g_oled_batch_depth--;
	if(g_oled_batch_depth == 0u)
		OLED_Refresh();
}

void OLED_WR_Byte(u8 dat,u8 mode)
{
	I2C_Start();
	Send_Byte(g_oled_i2c_waddr);
	if(!I2C_WaitAck()) { I2C_Stop(); return; }
	if(mode){Send_Byte(0x40);}
  else{Send_Byte(0x00);}
	if(!I2C_WaitAck()) { I2C_Stop(); return; }
	Send_Byte(dat);
	if(!I2C_WaitAck()) { I2C_Stop(); return; }
	I2C_Stop();
}

/* 一次 I2C 事务连续写显存：0x40 + len 字节，列地址自动 +1；避免逐字节起停导致首列/边沿异常 */
static void OLED_I2C_WriteDataBurst(const u8 *p, u16 len)
{
	u16 k;
	I2C_Start();
	Send_Byte(g_oled_i2c_waddr);
	if(!I2C_WaitAck()) { I2C_Stop(); return; }
	Send_Byte(0x40);
	if(!I2C_WaitAck()) { I2C_Stop(); return; }
	for(k = 0; k < len; k++)
	{
		Send_Byte(p[k]);
		if(!I2C_WaitAck()) { I2C_Stop(); return; }
	}
	I2C_Stop();
}

static void OLED_SetPageColumn(u8 page, u8 col)
{
	OLED_WR_Byte((u8)(0xB0u + page), OLED_CMD);
	OLED_WR_Byte((u8)((col & 0x0Fu) | (u8)(OLED_COLUMN_ADDR_LOW_OR & 0x0Fu)), OLED_CMD);
	OLED_WR_Byte((u8)(0x10u | ((col >> 4) & 0x0Fu)), OLED_CMD);
}

void OLED_SelfTest_PanelAndColumn(void)
{
	u8 y;
	char line0[17] = "L0|R127 EDGE    ";
	char line1[17] = "0123456789ABCDEF";
	char line2[17] = "PANEL:";
	char line3[17] = "OFF:";

	OLED_Clear();

	/* 画第0列与第127列竖线，快速观察首列是否缺失/偏移 */
	for(y = 0u; y < 64u; y++)
	{
		OLED_DrawPoint(0u, y);
		OLED_DrawPoint(127u, y);
	}

#if (OLED_PANEL_TYPE == 1u)
	line2[6] = 'S'; line2[7] = 'H'; line2[8] = '1'; line2[9] = '1'; line2[10] = '0'; line2[11] = '6';
#else
	line2[6] = 'S'; line2[7] = 'S'; line2[8] = 'D'; line2[9] = '1'; line2[10] = '3'; line2[11] = '0'; line2[12] = '6';
#endif

	line3[4] = (char)('0' + (OLED_COLUMN_OFFSET / 10u));
	line3[5] = (char)('0' + (OLED_COLUMN_OFFSET % 10u));
	line3[6] = ' '; line3[7] = 'A'; line3[8] = 'D'; line3[9] = 'D'; line3[10] = 'R';

	OLED_ShowString(0, 0, line0, 16);
	OLED_ShowString(0, 16, line1, 16);
	OLED_ShowString(0, 32, line2, 16);
	OLED_ShowString(0, 48, line3, 16);
	OLED_Refresh();
}


//????OLED??? 
void OLED_DisPlay_On(void)
{
	OLED_WR_Byte(0x8D,OLED_CMD);
	OLED_WR_Byte(0x14,OLED_CMD);
	OLED_WR_Byte(0xAF,OLED_CMD);
}

//???OLED??? 
void OLED_DisPlay_Off(void)
{
	OLED_WR_Byte(0x8D,OLED_CMD);
	OLED_WR_Byte(0x10,OLED_CMD);
	OLED_WR_Byte(0xAE,OLED_CMD);
}

//???????OLED	
void OLED_Refresh(void)
{
	u8 i,n;
	u8 col0;
#if defined(OLED_COLUMN_OFFSET)
	col0 = (u8)OLED_COLUMN_OFFSET;
#else
	col0 = 0u;
#endif
	for(i=0;i<8;i++)
	{
	   OLED_SetPageColumn(i, col0);
	   /* 列地址建立后再发首字节数据；长线/弱上拉时有利于首列稳定 */
	   delay_us(2);
	   for(n=0;n<128;n++)
			s_oled_refresh_line[n] = OLED_GRAM[n][i];
	   OLED_I2C_WriteDataBurst(s_oled_refresh_line, 128);
  }
}
//????????
void OLED_Clear(void)
{
	u8 i,n;
	for(i=0;i<8;i++)
	{
	   for(n=0;n<128;n++)
			{
			 OLED_GRAM[n][i]=0;
			}
  }
	g_oled_batch_depth = 0u;
	OLED_Refresh();
}

//???? 
//x:0~127
//y:0~63
void OLED_DrawPoint(u8 x,u8 y)
{
	u8 i,m,n;
	u16 xs;
#if defined(OLED_GRAM_X_SHIFT)
	xs = (u16)x + (u16)OLED_GRAM_X_SHIFT;
#else
	xs = x;
#endif
	if(xs >= 128u)
		return;
	i=y/8;
	m=y%8;
	n=1<<m;
	OLED_GRAM[(u8)xs][i]|=n;
}

//x:0~127
//y:0~63
void OLED_ClearPoint(u8 x,u8 y)
{
	u8 i, m;
	u16 xs;
#if defined(OLED_GRAM_X_SHIFT)
	xs = (u16)x + (u16)OLED_GRAM_X_SHIFT;
#else
	xs = x;
#endif
	i = (u8)(y / 8u);
	m = (u8)(y % 8u);
	if(xs >= 128u || i >= 8u)
		return;
	OLED_GRAM[(u8)xs][i] &= (u8)~(1u << m);
}

void OLED_ShowChinese16(u8 x,u8 y,u8 num,u8 size1)
{
	u8 x1=x,x2=x+8,y1=y,y2=y;  //??????
	u8 i,j,temp;
	for(i=0;i<2*size1;i++)
	{
		if(i%2==0)
		{
			x1=x;
			temp=hysy16[num][i];
			for(j=0;j<8;j++)
			{
				if(temp&0x01)OLED_DrawPoint(x1,y1);
				else OLED_ClearPoint(x1,y1);
				temp>>=1;
				x1++;
			}
			y1=y1+1;
		}
		else
		{
			x2=x+8;
			temp=hysy16[num][i];
			for(j=0;j<8;j++)
			{
				if(temp&0x01)OLED_DrawPoint(x2,y2);
				else OLED_ClearPoint(x2,y2);
				temp>>=1;
				x2++;
			}
			y2=y2+1;
		}
	}
	
	if(g_oled_batch_depth == 0u)
		OLED_Refresh();
}
void OLED_ShowChinese32(u8 x,u8 y,u8 num,u8 size1)
{
	u8 x1=x,x2=x+8,x3=x+16,x4=x+24;
	u8 y1=y,y2=y,y3=y,y4=y;  //??????
	u8 i,j,temp;
	for(i=0;i<4*size1;i++)
	{
		if(i%4==0)
		{
			x1=x;
			temp=yxjs32[num][i];
			for(j=0;j<8;j++)
			{
				if(temp&0x01)OLED_DrawPoint(x1,y1);
				else OLED_ClearPoint(x1,y1);
				temp>>=1;
				x1++;
			}
			y1=y1+1;
		}
		if(i%4==1)
		{
			x2=x+8;
			temp=yxjs32[num][i];
			for(j=0;j<8;j++)
			{
				if(temp&0x01)OLED_DrawPoint(x2,y2);
				else OLED_ClearPoint(x2,y2);
				temp>>=1;
				x2++;
			}
			y2=y2+1;
		}
		if(i%4==2)
		{
			x3=x+16;
			temp=yxjs32[num][i];
			for(j=0;j<8;j++)
			{
				if(temp&0x01)OLED_DrawPoint(x3,y3);
				else OLED_ClearPoint(x3,y3);
				temp>>=1;
				x3++;
			}
			y3=y3+1;
		}
		if(i%4==3)
		{
			x4=x+24;
			temp=yxjs32[num][i];
			for(j=0;j<8;j++)
			{
				if(temp&0x01)OLED_DrawPoint(x4,y4);
				else OLED_ClearPoint(x4,y4);
				temp>>=1;
				x4++;
			}
			y4=y4+1;
		}
	}
	
	if(g_oled_batch_depth == 0u)
		OLED_Refresh();
}
void OLED_ShowChinese(u8 x,u8 y,u8 num,u8 size1)
{
	u8 X=x,Y=y,NUM=num,SIZE=size1;
	switch(size1)
	{
		case 16: OLED_ShowChinese16(X,Y,NUM,SIZE);break;
		case 32: OLED_ShowChinese32(X,Y,NUM,SIZE);break;
	}
}
void OLED_ShowCharu8(u8 x,u8 y,u8 chr,u8 size1)
{
	u8 i,m,temp,size2,chr1;
	u8 y0=y;
	size2=(size1/8+((size1%8)?1:0))*(size1/2);  //??????????????????????????????
	chr1=chr-' ';
	for(i=0;i<size2;i++)
	{
		if(size1==12)
        {temp=asc2_1206[chr1][i];} //????1206????
		else if(size1==16)
        {temp=asc2_1608[chr1][i];} //????1608????
		else return;
				for(m=0;m<8;m++)           //???????
				{
					if(temp&0x80)OLED_DrawPoint(x,y);
					else OLED_ClearPoint(x,y);
					temp<<=1;
					y++;
					if((y-y0)==size1)
					{
						y=y0;
						x++;
						break;
          }
				}
  }
}
//????????
//x,y:???????? 
//size1:???????
//*chr:??????????? 
void OLED_ShowStringu8(u8 x,u8 y,u8 *chr,u8 size1)
{
	u8 x0 = x;
	u8 cnt = 0;
	while((*chr>=' ')&&(*chr<='~'))//?????????????!
	{
		OLED_ShowCharu8(x,y,*chr,size1);
		x+=size1/2;
		cnt++;
		if(x>128-size1)  //????
		{
			x=0;
			y+=2;
    }
		chr++;
  }
	/* 常用16px整行文案：自动补空格到16列，避免短字符串覆盖长字符串后残留 */
	if((size1==16) && (x0==0) && ((y%16)==0))
	{
		while(cnt < 16)
		{
			OLED_ShowCharu8((u8)(cnt*(size1/2)), y, ' ', size1);
			cnt++;
		}
	}
	if(g_oled_batch_depth == 0u)
		OLED_Refresh();
}

//?????????????????,???????????
//x:0~127
//y:0~63
//size:??????? 12/16/24
void OLED_ShowChar(u8 x,u8 y,char chr,u8 size1)
{
	u8 i,m,temp,size2,chr1;
	u8 y0=y;
	size2=(size1/8+((size1%8)?1:0))*(size1/2);  //??????????????????????????????
	chr1=chr-' ';
	for(i=0;i<size2;i++)
	{
		if(size1==12)
        {temp=asc2_1206[chr1][i];} //????1206????
		else if(size1==16)
        {temp=asc2_1608[chr1][i];} //????1608????
		else return;
				for(m=0;m<8;m++)           //???????
				{
					if(temp&0x80)OLED_DrawPoint(x,y);
					else OLED_ClearPoint(x,y);
					temp<<=1;
					y++;
					if((y-y0)==size1)
					{
						y=y0;
						x++;
						break;
          }
				}
  }
}

//????????
//x,y:???????? 
//size1:???????
//*chr:??????????? 
void OLED_ShowString(u8 x,u8 y,char chr[],u8 size1)
{
	unsigned char i=0;
	u8 x0 = x;
	while((chr[i]>=' ')&&(chr[i]<='~'))//?????????????!
	{
		OLED_ShowChar(x,y,chr[i],size1);
		x+=size1/2;
		if(x>128-size1)  //????
		{
			x=0;
			y+=2;
    }
		i++;
  }
	/* 常用16px整行文案：自动补空格到16列，避免短字符串覆盖长字符串后残留 */
	if((size1==16) && (x0==0) && ((y%16)==0))
	{
		while(i < 16)
		{
			OLED_ShowChar((u8)(i*(size1/2)), y, ' ', size1);
			i++;
		}
	}
	if(g_oled_batch_depth == 0u)
		OLED_Refresh();
}

//m^n
u32 OLED_Pow(u8 m,u8 n)
{
	u32 result=1;
	while(n--)
	{
	  result*=m;
	}
	return result;
}

//??????????????????
void OLED_WR_BP(u8 x,u8 y)
{
	u8 xa;
#if defined(OLED_COLUMN_OFFSET)
	xa = (u8)(x + (u8)OLED_COLUMN_OFFSET);
#else
	xa = x;
#endif
	OLED_SetPageColumn(y, xa);
}

//x0,y0??????????//x1,y1?????????

void OLED_ShowPicture(u8 x0,u8 y0,u8 x1,u8 y1,u8 BMP[])
{
	u32 j=0;
	u8 x=0,y=0;
	if(y%8==0)y=0;
	else y+=1;
	for(y=y0;y<y1;y++)
	 {
		 OLED_WR_BP(x0,y);
		 for(x=x0;x<x1;x++)
		 {
			 OLED_WR_Byte(BMP[j],OLED_DATA);
			 j++;
     }
	 }
}
//OLED??????
void OLED_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
 	RCC_APB2PeriphClockCmd(BOARD_OLED_GPIO_CLK, ENABLE);	 //???A??????
	GPIO_InitStructure.GPIO_Pin = BOARD_OLED_PIN_SCL | BOARD_OLED_PIN_SDA;	 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; 		 //????????I2C ??????????????????
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//???50MHz
 	GPIO_Init(BOARD_OLED_GPIO_PORT, &GPIO_InitStructure);	  
 	GPIO_SetBits(BOARD_OLED_GPIO_PORT, BOARD_OLED_PIN_SCL | BOARD_OLED_PIN_SDA);		
	oled_i2c_bus_recovery();
	delay_ms(200);

	/* probe 0x78/0x7A, retry after bus recovery */
	g_oled_probe_ok = 0;
	if (oled_i2c_probe_raw(0x78)) {
		g_oled_i2c_waddr = 0x78;
		g_oled_probe_ok = 1;
	} else if (oled_i2c_probe_raw(0x7A)) {
		g_oled_i2c_waddr = 0x7A;
		g_oled_probe_ok = 1;
	} else {
		delay_ms(120);
		oled_i2c_bus_recovery();
		if (oled_i2c_probe_raw(0x78)) {
			g_oled_i2c_waddr = 0x78;
			g_oled_probe_ok = 1;
		} else if (oled_i2c_probe_raw(0x7A)) {
			g_oled_i2c_waddr = 0x7A;
			g_oled_probe_ok = 1;
		} else
			g_oled_i2c_waddr = OLED_I2C_ADDR;
	}
	
	OLED_WR_Byte(0xAE,OLED_CMD);//--turn off oled panel
	OLED_WR_Byte(0x00,OLED_CMD);//---set low column address
	OLED_WR_Byte(0x10,OLED_CMD);//---set high column address
	OLED_WR_Byte(0x40,OLED_CMD);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
	OLED_WR_Byte(0x81,OLED_CMD);//--set contrast control register
	OLED_WR_Byte(0xFF,OLED_CMD);// Set SEG Output Current Brightness
	OLED_WR_Byte(0xA1,OLED_CMD);//--Set SEG/Column Mapping     0xa0??????? 0xa1????
	OLED_WR_Byte(0xC8,OLED_CMD);//Set COM/Row Scan Direction   0xc0??????? 0xc8????
	OLED_WR_Byte(0xA6,OLED_CMD);//--set normal display
	OLED_WR_Byte(0xA8,OLED_CMD);//--set multiplex ratio(1 to 64)
	OLED_WR_Byte((OLED_HEIGHT==32)?0x1F:0x3F,OLED_CMD);//--1/32 or 1/64 duty
	OLED_WR_Byte(0xD3,OLED_CMD);//-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
	OLED_WR_Byte(0x00,OLED_CMD);//-not offset
	OLED_WR_Byte(0xd5,OLED_CMD);//--set display clock divide ratio/oscillator frequency
	OLED_WR_Byte(0x80,OLED_CMD);//--set divide ratio, Set Clock as 100 Frames/Sec
	OLED_WR_Byte(0xD9,OLED_CMD);//--set pre-charge period
	OLED_WR_Byte(0xF1,OLED_CMD);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
	OLED_WR_Byte(0xDA,OLED_CMD);//--set com pins hardware configuration
	OLED_WR_Byte((OLED_HEIGHT==32)?0x02:0x12,OLED_CMD);
	OLED_WR_Byte(0xDB,OLED_CMD);//--set vcomh
	OLED_WR_Byte(0x40,OLED_CMD);//Set VCOM Deselect Level
	OLED_WR_Byte(0x20,OLED_CMD);//-Set Page Addressing Mode (0x00/0x01/0x02)
	OLED_WR_Byte(0x02,OLED_CMD);//
	OLED_WR_Byte(0x8D,OLED_CMD);//--set Charge Pump enable/disable
	OLED_WR_Byte(0x14,OLED_CMD);//--set(0x10) disable
	OLED_WR_Byte(0xA4,OLED_CMD);// Disable Entire Display On (0xa4/0xa5)
	OLED_WR_Byte(0xA6,OLED_CMD);// Disable Inverse Display On (0xa6/a7) 
	OLED_WR_Byte(0xAF,OLED_CMD);
	OLED_Clear();
}

void huanying(void)
{
	OLED_ShowChinese(5,20,0,16);    //??
	OLED_ShowChinese(32+5,20,1,16);   //?
	OLED_ShowChinese(64+5,20,2,16);   //?
	OLED_ShowChinese(96+5,20,3,16);   //??
	delay_ms(1500);
}

