#include "sccb.h"
#include "delay.h"

/*
 * 无外部上拉时的 SCCB（类似 I2C）软件实现：
 * - 低电平：开漏输出拉低
 * - 高电平：切到输入上拉（使用 MCU 内部上拉）
 * 这样即使没有外接 4.7k~10k 上拉，也能跑通识别/初始化（速度更慢、更易兼容）。
 */

static void SCCB_SDA_OUT_OD(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Pin = SCCB_SDA_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(SCCB_SDA_PORT,&GPIO_InitStructure);
}

static void SCCB_SDA_IN_PU(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Pin = SCCB_SDA_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_Init(SCCB_SDA_PORT,&GPIO_InitStructure);
}

static void SCCB_SCL_OUT_OD(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Pin = SCCB_SCL_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(SCCB_SCL_PORT,&GPIO_InitStructure);
}

static void SCCB_SCL_IN_PU(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Pin = SCCB_SCL_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_Init(SCCB_SCL_PORT,&GPIO_InitStructure);
}

static void sccb_sda_hi(void)
{
		SCCB_SDA_IN_PU(); /* 释放为高 */
		delay_us(10);
}

static void sccb_sda_lo(void)
{
		SCCB_SDA_OUT_OD();
		SCCB_SDA(0);
		delay_us(10);
}

static void sccb_scl_hi(void)
{
		SCCB_SCL_IN_PU(); /* 释放为高 */
		delay_us(10);
}

static void sccb_scl_lo(void)
{
		SCCB_SCL_OUT_OD();
		SCCB_SCL(0);
		delay_us(10);
}

void SCCB_Init(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
		RCC_APB2PeriphClockCmd(SCCB_GPIO_CLK,ENABLE);

		GPIO_InitStructure.GPIO_Pin = SCCB_SCL_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(SCCB_SCL_PORT,&GPIO_InitStructure);

		GPIO_InitStructure.GPIO_Pin = SCCB_SDA_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_Init(SCCB_SDA_PORT,&GPIO_InitStructure);

		/* 空闲态：两线释放为高（内部上拉） */
		sccb_scl_hi();
		sccb_sda_hi();
}

static void SCCB_Start(void)
{
		sccb_sda_hi();
		sccb_scl_hi();
		delay_us(80);
		sccb_sda_lo();
		delay_us(80);
		sccb_scl_lo();
}

static void SCCB_Stop(void)
{
		sccb_sda_lo();
		delay_us(80);
		sccb_scl_hi();
		delay_us(80);
		sccb_sda_hi();
		delay_us(80);
}

static void SCCB_NoAck(void)
{
		delay_us(80);
		sccb_sda_hi();
		sccb_scl_hi();
		delay_us(80);
		sccb_scl_lo();
		delay_us(80);
		sccb_sda_lo();
		delay_us(80);
}

static u8 SCCB_WR_Byte(u8 dat)
{
		u8 j,res;
		for(j=0;j<8;j++)
		{
				if(dat&0x80) sccb_sda_hi();
				else sccb_sda_lo();
				dat<<=1;
				delay_us(60);
				sccb_scl_hi();
				delay_us(60);
				sccb_scl_lo();
		}
		SCCB_SDA_IN_PU();
		delay_us(60);
		sccb_scl_hi();
		delay_us(60);
		res = (SCCB_READ_SDA)?1:0;
		sccb_scl_lo();
		SCCB_SDA_OUT_OD();
		return res;
}

static u8 SCCB_RD_Byte(void)
{
		u8 temp=0,j;
		SCCB_SDA_IN_PU();
		for(j=8;j>0;j--)
		{
				delay_us(60);
				sccb_scl_hi();
				temp<<=1;
				if(SCCB_READ_SDA) temp++;
				delay_us(60);
				sccb_scl_lo();
		}
		SCCB_SDA_OUT_OD();
		return temp;
}

u8 SCCB_WR_Reg(u8 reg,u8 data)
{
		u8 res=0;
		SCCB_Start();
		if(SCCB_WR_Byte(SCCB_ID)) res=1;
		delay_us(100);
		if(SCCB_WR_Byte(reg)) res=1;
		delay_us(100);
		if(SCCB_WR_Byte(data)) res=1;
		SCCB_Stop();
		return res;
}

u8 SCCB_RD_Reg(u8 reg)
{
		u8 val;
		SCCB_Start();
		SCCB_WR_Byte(SCCB_ID);
		delay_us(100);
		SCCB_WR_Byte(reg);
		delay_us(100);
		SCCB_Stop();
		delay_us(100);
		SCCB_Start();
		SCCB_WR_Byte(SCCB_ID|0X01);
		delay_us(100);
		val = SCCB_RD_Byte();
		SCCB_NoAck();
		SCCB_Stop();
		return val;
}

