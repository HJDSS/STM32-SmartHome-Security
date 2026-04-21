#include "mq2.h"
#include "delay.h"
#include "stm32f10x_adc.h"

/* 滞回锁存：上电或调用 MQ2_Alarm_Latch_Reset 前勿误判 */
static u8 s_mq2_alarm_latch = 0;

void MQ2_Alarm_Latch_Reset(void)
{
	s_mq2_alarm_latch = 0;
}

void MQ2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_APB2PeriphClockCmd(MQ2_ADC_GPIO_CLK | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6); // 72MHz/6=12MHz

    GPIO_InitStructure.GPIO_Pin = MQ2_ADC_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(MQ2_ADC_GPIO_PORT, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

u16 MQ2_Read_ADC_Once(void)
{
    ADC_RegularChannelConfig(ADC1, MQ2_ADC_CHANNEL, 1, MQ2_ADC_SAMPLE_TIME);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    return ADC_GetConversionValue(ADC1);
}

u16 MQ2_Read_ADC_Filter(void)
{
    u32 sum = 0;
    u8 i;

    for(i=0; i<MQ2_FILTER_SAMPLE_NUM; i++)
    {
        sum += MQ2_Read_ADC_Once();
        delay_ms(2);
    }
    return (u16)(sum / MQ2_FILTER_SAMPLE_NUM);
}

u8 MQ2_Check_Alarm(u16 adc_value)
{
	if (s_mq2_alarm_latch == 0)
	{
		if (adc_value >= MQ2_ALARM_HIGH_THRESHOLD)
			s_mq2_alarm_latch = 1;
	}
	else
	{
		if (adc_value <= MQ2_ALARM_LOW_THRESHOLD)
			s_mq2_alarm_latch = 0;
	}
	return s_mq2_alarm_latch;
}

float MQ2_Get_Value(void)
{
    u16 v = MQ2_Read_ADC_Filter();
    return (float)v * (100.0f / 4095.0f);
}

