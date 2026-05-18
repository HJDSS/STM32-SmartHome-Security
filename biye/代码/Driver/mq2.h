#ifndef __MQ2_H
#define __MQ2_H

#include "board_config.h"

/* MQ-2 ADC：引脚与通道见 board_config.h 中 BOARD_MQ2_*（单芯片规范：PB0/ADC1_IN8） */
#define MQ2_ADC_GPIO_PORT               BOARD_MQ2_ADC_GPIO_PORT
#define MQ2_ADC_GPIO_PIN                BOARD_MQ2_ADC_GPIO_PIN
#define MQ2_ADC_GPIO_CLK                BOARD_MQ2_ADC_GPIO_CLK
#define MQ2_ADC_CHANNEL                 BOARD_MQ2_ADC_CHANNEL
#define MQ2_ADC_SAMPLE_TIME             BOARD_MQ2_ADC_SAMPLE_TIME

#define MQ2_ALARM_HIGH_THRESHOLD        2400
#define MQ2_ALARM_LOW_THRESHOLD         2000

/* 自适应基线 — 论文公式 Abase(k)=0.9*Abase(k-1)+0.1*Ak (Q8定点) */
#define MQ2_BASELINE_WARMUP_SAMPLES    100u    /* 基线就绪前需采集样本数 */
#define MQ2_BASELINE_ALPHA_Q8          230u    /* 0.9 in Q8 (230/256 ≈ 0.898) */
#define MQ2_SAFETY_MARGIN_ADC          40u     /* 安全裕量(ADC counts) */

void MQ2_Init(void);
void MQ2_Alarm_Latch_Reset(void);
u16 MQ2_Read_ADC_Once(void);
u16 MQ2_Read_ADC_Filter(void);
u16 MQ2_ApplyTempComp(u16 adc_raw, int16_t temp_c);  /* 温漂补偿: Acomp=Araw-3.2*(T-25), 论文公式4.3 */
u8 MQ2_Check_Alarm(u16 adc_value);
void MQ2_UpdateBaseline(u16 adc_filtered);  /* 自适应基线更新(每次传感器周期调用) */

/* 论文表3-2 多浓度点标定点数 */
#define MQ2_CAL_POINTS                  5u

/* 分段线性插值：将 ADC 值转换为浓度 (ppm)
 * 论文第三章多浓度点标定，使用 5 点分段线性化
 * 参数: adc — 经 MQ2_Read_ADC_Filter() 滤波后的 ADC 值
 * 返回: 浓度 ppm (0~3000)
 */
u16 MQ2_Get_Value(u16 adc);

#endif
