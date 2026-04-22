#include "ov7670_fifo.h"
#include "board_config.h"
#include "sccb.h"
#include "ov7670cfg_qvga_rgb565.h"
#include "delay.h"

static volatile u8 s_ov_pid = 0xFF;
static volatile u8 s_ov_ver = 0xFF;

/* ov_gpio_init：引脚来自 board_config.h（单芯片 RCT6）；PA/PB 与 FIFO/并口一致 */
static void ov_gpio_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	/* 使能 PA(VSYNC/RCK/数据)、PB(FIFO 控制、WREN/OE、WRST/RRST) 与 AFIO */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);

	GPIO_InitStructure.GPIO_Pin = OV_VSYNC_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(OV_VSYNC_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = OV_RCK_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(OV_RCK_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = OV_OE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(OV_OE_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = OV_WRST_PIN | OV_RRST_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(OV_WRST_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = OV_WREN_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(OV_WREN_PORT, &GPIO_InitStructure);

	/* 数据口 D0~D7：PA0~PA7 上拉输入 */
	GPIO_InitStructure.GPIO_Pin = 0x00FF;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(OV_DATA_PORT, &GPIO_InitStructure);

	OV_OE(1);
	OV_WREN(1);
	OV_RCK_H();
	OV_WRST(1);
	OV_RRST(1);
}

u8 OV7670_FIFO_Init(void)
{
	u8 temp;
	u16 i;

	ov_gpio_init();
	SCCB_Init();

	if(SCCB_WR_Reg(0x12, 0x80)) return 1;
	delay_ms(50);

	temp = SCCB_RD_Reg(0x0b);
	s_ov_pid = temp;
	if(temp != 0x73) return 2;
	temp = SCCB_RD_Reg(0x0a);
	s_ov_ver = temp;
	if(temp != 0x76) return 2;

	for(i = 0; i < OV7670_INIT_REG_TBL_LEN; i++)
	{
		SCCB_WR_Reg(ov7670_init_reg_tbl[i][0], ov7670_init_reg_tbl[i][1]);
	}
	return 0;
}

void OV7670_DebugGetId(u8 *pid, u8 *ver)
{
	if(pid) *pid = (u8)s_ov_pid;
	if(ver) *ver = (u8)s_ov_ver;
}

void OV7670_FIFO_StartCapture(void)
{
	OV_WRST(0);
	delay_us(5);
	OV_WRST(1);
	delay_us(5);

	OV_WREN(0);
	while(OV_VSYNC_READ() == 0);
	while(OV_VSYNC_READ() == 1);
	OV_WREN(1);
}

void OV7670_FIFO_ResetReadPtr(void)
{
	OV_RRST(0);
	delay_us(5);
	OV_RCK_L();
	delay_us(5);
	OV_RCK_H();
	delay_us(5);
	OV_RRST(1);
	OV_OE(0);
}

u8 OV7670_FIFO_ReadByte(void)
{
	u8 dat;
	OV_RCK_L();
	dat = OV_DATA_READ();
	OV_RCK_H();
	return dat;
}
