#ifndef __MQ2_H
#define __MQ2_H

#include "board_config.h"

/* MQ-2 ADC：引脚与通道见 board_config.h 中 BOARD_MQ2_*（单芯片规范：PB0/ADC1_IN8） */
#define MQ2_ADC_GPIO_PORT               BOARD_MQ2_ADC_GPIO_PORT
#define MQ2_ADC_GPIO_PIN                BOARD_MQ2_ADC_GPIO_PIN
#define MQ2_ADC_GPIO_CLK                BOARD_MQ2_ADC_GPIO_CLK
#define MQ2_ADC_CHANNEL                 BOARD_MQ2_ADC_CHANNEL
#define MQ2_ADC_SAMPLE_TIME             BOARD_MQ2_ADC_SAMPLE_TIME

#define MQ2_FILTER_SAMPLE_NUM           8
#define MQ2_ALARM_HIGH_THRESHOLD        2400
#define MQ2_ALARM_LOW_THRESHOLD         2000

void MQ2_Init(void);
void MQ2_Alarm_Latch_Reset(void);
u16 MQ2_Read_ADC_Once(void);
u16 MQ2_Read_ADC_Filter(void);
u8 MQ2_Check_Alarm(u16 adc_value);

/* 本地 ADC浓度相关量（0.0~100.0 线性占满度，ppm 需标定；内部已做滤波采样） */
float MQ2_Get_Value(void);

#endif
