#include "mq2.h"
#include "stm32f10x_adc.h"

/* 滞回锁存：上电或调用 MQ2_Alarm_Latch_Reset 前勿误判 */
static u8 s_mq2_alarm_latch = 0;

/* 自适应基线 — 论文公式 Abase(k)=0.9*Abase(k-1)+0.1*Ak (Q8定点) */
static u32 s_gas_baseline_q8 = 0;      /* Q8 基线估计 */
static u32 s_gas_variance_q8 = 0;      /* Q8 滑动方差 */
static u32 s_gas_baseline_age = 0;     /* 基线样本计数 */
static u8  s_gas_baseline_ready = 0;   /* 基线是否就绪 */

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

/* MQ-2 中值+滑动平均级联滤波（论文 §4.3）*/
u16 MQ2_Read_ADC_Filter(void)
{
    u8 i, j;
    u16 tmp;
    u16 raw_buf[APP_MQ2_MEDIAN_WIN];       /* 中值滤波窗口 */
    static u16 ma_buf[APP_MQ2_MA_WIN] = {0}; /* 滑动平均历史 (static 跨调用保持) */
    static u8 ma_idx = 0;
    u32 sum;

    /* 第1级：采集 APP_MQ2_MEDIAN_WIN 个样本 */
    for (i = 0; i < APP_MQ2_MEDIAN_WIN; i++) {
        raw_buf[i] = MQ2_Read_ADC_Once();
    }

    /* 中值滤波（冒泡排序后取中值） */
    for (i = 0; i < APP_MQ2_MEDIAN_WIN - 1; i++) {
        for (j = 0; j < APP_MQ2_MEDIAN_WIN - 1 - i; j++) {
            if (raw_buf[j] > raw_buf[j + 1]) {
                tmp          = raw_buf[j];
                raw_buf[j]   = raw_buf[j + 1];
                raw_buf[j + 1] = tmp;
            }
        }
    }
    u16 median_val = raw_buf[APP_MQ2_MEDIAN_WIN / 2];

    /* 第2级：滑动平均 */
    ma_buf[ma_idx] = median_val;
    ma_idx = (ma_idx + 1) % APP_MQ2_MA_WIN;

    sum = 0;
    for (i = 0; i < APP_MQ2_MA_WIN; i++) {
        sum += ma_buf[i];
    }

    return (u16)(sum / APP_MQ2_MA_WIN);
}

/* 温漂补偿：Acomp = Araw - 3.2*(T-25)，论文公式4.3
 * temp_c: DHT11 温度（摄氏度），整数值（如25表示25°C）
 * adc_raw: MQ2_Read_ADC_Filter() 的输出
 * 返回：补偿后的 ADC 值
 */
u16 MQ2_ApplyTempComp(u16 adc_raw, int16_t temp_c)
{
	/* delta_T = temp_c - 25，可能是负数 */
	int16_t delta_T = temp_c - (int16_t)APP_MQ2_TEMP_BASE_C;

	/* compensation = 3.2 * delta_T in Q8, then round to integer */
	int32_t comp_q8 = (int32_t)APP_MQ2_TEMP_K_Q8 * (int32_t)delta_T;
	int16_t comp = (int16_t)(comp_q8 >> 8);  /* Q8 → integer */

	/* Acomp = Araw - comp (subtract because gas sensors read higher when hot) */
	int32_t result = (int32_t)adc_raw - (int32_t)comp;

	if (result < 0) result = 0;
	if (result > 4095) result = 4095;
	return (u16)result;
}

/* 整数平方根（逐位逼近法，O(log n)，无乘除，适合Cortex-M3无FPU） */
static u32 isqrt_u32(u32 x)
{
	u32 res = 0;
	u32 bit = 1u << 30;
	while (bit > x) bit >>= 2;
	while (bit != 0) {
		if (x >= res + bit) {
			x -= res + bit;
			res = (res >> 1) + bit;
		} else {
			res >>= 1;
		}
		bit >>= 2;
	}
	return res;
}

/* 更新自适应基线（每次传感器周期调用，~100ms间隔）
 * 论文公式 Abase(k)=0.9*Abase(k-1)+0.1*Ak
 * 前 MQ2_BASELINE_WARMUP_SAMPLES 个样本用于初始化基线
 */
void MQ2_UpdateBaseline(u16 adc_filtered)
{
	u32 ak_q8 = (u32)adc_filtered << 8;  /* Q8 */

	if (s_gas_baseline_age < MQ2_BASELINE_WARMUP_SAMPLES) {
		/* 初始阶段：累加平均建立基线 */
		s_gas_baseline_q8 = (s_gas_baseline_q8 * s_gas_baseline_age + ak_q8) / (s_gas_baseline_age + 1);
		s_gas_baseline_age++;
		if (s_gas_baseline_age >= MQ2_BASELINE_WARMUP_SAMPLES)
			s_gas_baseline_ready = 1;
	} else {
		/* IIR跟踪：Abase(k) = 0.9*Abase(k-1) + 0.1*Ak */
		u32 prev = s_gas_baseline_q8;
		s_gas_baseline_q8 = (MQ2_BASELINE_ALPHA_Q8 * prev + (256u - MQ2_BASELINE_ALPHA_Q8) * ak_q8) >> 8;

		/* 滑动方差：Var(k) = 0.9*Var(k-1) + 0.1*(Ak - Abase)^2 */
		u32 diff = (ak_q8 >= s_gas_baseline_q8) ? (ak_q8 - s_gas_baseline_q8) : (s_gas_baseline_q8 - ak_q8);
		u32 diff_sq = (diff * diff) >> 8;  /* Q8 */
		s_gas_variance_q8 = (MQ2_BASELINE_ALPHA_Q8 * s_gas_variance_q8 + (256u - MQ2_BASELINE_ALPHA_Q8) * diff_sq) >> 8;
	}
}

u8 MQ2_Check_Alarm(u16 adc_value)
{
	u16 high_thresh = MQ2_ALARM_HIGH_THRESHOLD;
	u16 low_thresh  = MQ2_ALARM_LOW_THRESHOLD;

	/* 基线就绪后切换为自适应阈值 */
	if (s_gas_baseline_ready) {
		/* 自适应阈值：Tgas = Abase + 3*std_dev + safety_margin */
		u32 std_q8 = isqrt_u32(s_gas_variance_q8) << 4;
		u32 threshold_q8 = s_gas_baseline_q8 + 3u * std_q8 + ((u32)MQ2_SAFETY_MARGIN_ADC << 8);
		u16 adaptive_high = (u16)(threshold_q8 >> 8);

		/* 下限保护：不低于 MQ2_ALARM_LOW_THRESHOLD */
		if (adaptive_high < MQ2_ALARM_LOW_THRESHOLD)
			adaptive_high = MQ2_ALARM_LOW_THRESHOLD;

		high_thresh = adaptive_high;
		/* 滞回下限：取自适应阈值的90%或固定下限的较高者 */
		{
			u16 hyst_low = (u16)(((u32)adaptive_high * 9u) / 10u);
			low_thresh = (hyst_low > MQ2_ALARM_LOW_THRESHOLD) ? hyst_low : MQ2_ALARM_LOW_THRESHOLD;
		}
	}

	if (s_mq2_alarm_latch == 0)
	{
		if (adc_value >= high_thresh)
			s_mq2_alarm_latch = 1;
	}
	else
	{
		if (adc_value <= low_thresh)
			s_mq2_alarm_latch = 0;
	}
	return s_mq2_alarm_latch;
}

/* 论文表3-2 五浓度点标定数据 (ADC mid-point, ppm) */
static const u16 cal_adc[MQ2_CAL_POINTS] = {175, 448, 898, 1515, 2200};
static const u16 cal_ppm[MQ2_CAL_POINTS] = {0,   300, 1000, 2000, 3000};

/* 分段线性插值，将 ADC 值转换为浓度 (ppm)
 * 论文第三章多浓度点标定
 */
u16 MQ2_Get_Value(u16 adc)
{
    u8 i;
    if (adc <= cal_adc[0]) return 0u;
    if (adc >= cal_adc[MQ2_CAL_POINTS - 1]) return cal_ppm[MQ2_CAL_POINTS - 1];

    for (i = 0; i < MQ2_CAL_POINTS - 1; i++) {
        if (adc >= cal_adc[i] && adc < cal_adc[i + 1]) {
            /* 标定点 i 和 i+1 之间线性插值 */
            u32 adc_range = cal_adc[i + 1] - cal_adc[i];
            u32 ppm_range = cal_ppm[i + 1] - cal_ppm[i];
            u32 offset_adc = adc - cal_adc[i];
            return (u16)(cal_ppm[i] + (offset_adc * ppm_range) / adc_range);
        }
    }
    return cal_ppm[MQ2_CAL_POINTS - 1];
}

