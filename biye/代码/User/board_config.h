#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include "sys.h"

/*
 * =============================================================================
 * 目标芯片：STM32F103RCT6（LQFP64，Cortex-M3 @72MHz）
 * 工程形态：单芯片智能安防（标准外设库，裸机 super-loop，无 FreeRTOS）
 * 调试下载：关 JTAG、**始终保留 SWD**（PA13/PA14），便于烧录与在线调试
 * IO策略：SCCB 与 OV FIFO OE 分离（PD2/PB7 与 PB6）；ESP8266 用 USART3（PB10/11）+ EN(PC9)，PA0~7 专供 OV 数据总线
 * 本版**不包含**通风、空调继电器功能（无对应引脚分配与联动）
 * =============================================================================
 */

/* ================= 功能总开关（保持原工程） ================= */
#ifndef NO_FREERTOS_MODE
#define NO_FREERTOS_MODE    1       /* 固定为 1：裸机轮询（摄像头/联网等在主循环内调度） */
#endif
/* DEBUG_KEYBOARD_ONLY=1：仅 OLED+蜂鸣器+键盘；DEBUG_PASSWORD_ONLY=1：加密码+指纹，无 WiFi/MQ2/DHT/PIR/OV 等；二者与全功能互斥 */
#ifndef DEBUG_KEYBOARD_ONLY
/* 1=仅 OLED+蜂鸣器+矩阵键盘单测（不跑密码/指纹/WiFi/摄像头等）；全功能固件保持 0 */
#define DEBUG_KEYBOARD_ONLY  0
#endif
#ifndef DEBUG_PASSWORD_ONLY
/* [M1] 默认由 1 改为 0：全功能裸机为出厂默认。
 * 需要重回"密码+指纹"专项调试时，在编译器/命令行覆盖 -DDEBUG_PASSWORD_ONLY=1 即可，
 * 或在本头文件此宏上方新增 `#define DEBUG_PASSWORD_ONLY 1`。 */
#define DEBUG_PASSWORD_ONLY  0      /* 1=密码+指纹调试；会强制 DEBUG_KEYBOARD_ONLY=0 与 NO_FREERTOS_MODE=1 */
#endif
#if DEBUG_PASSWORD_ONLY
#undef NO_FREERTOS_MODE
#define NO_FREERTOS_MODE  1
#undef DEBUG_KEYBOARD_ONLY
#define DEBUG_KEYBOARD_ONLY  0
#endif
#if DEBUG_KEYBOARD_ONLY
#undef NO_FREERTOS_MODE
#define NO_FREERTOS_MODE  1
/* 单测主循环里 Key_Scan 周期间隔(ms)；略小于全功能 20ms，便于连续两次采样通过 KEY_STABLE_MATCHES */
#ifndef DEBUG_KEYBOARD_POLL_MS
#define DEBUG_KEYBOARD_POLL_MS  5u
#endif
#endif
#ifndef DEBUG_KEYBOARD_POLL_MS
#define DEBUG_KEYBOARD_POLL_MS  20u
#endif
/* 密码调试模式下是否编译/运行 WiFi·ESP8266·OneNET：
 * 0=暂关（先调本地）
 * 1=恢复联网调试
 * 当前默认：已从 NodeMCU 切到 ESP-01，恢复启用 ESP8266 以便直接联调。
 */
#ifndef DEBUG_PASSWORD_ESP8266_ENABLE
#define DEBUG_PASSWORD_ESP8266_ENABLE   0
#endif
/* 1=仅保留 DHT11 + OLED 诊断页（用于单独调试传感器），其余功能在主循环中不运行 */
#ifndef DHT11_ONLY_TEST
#define DHT11_ONLY_TEST                0
#endif

#if DEBUG_KEYBOARD_ONLY || DEBUG_PASSWORD_ONLY
/* 轻量调试模式：关闭重模块；密码调试保留指纹与 SD */
#undef EN_OV7670_LOCAL
#undef EN_FATFS_SD
#undef EN_AS608
#undef EN_ESP8266_ONENET
#if DEBUG_PASSWORD_ONLY
#define EN_OV7670_LOCAL     1
#else
#define EN_OV7670_LOCAL     0
#endif
#if DEBUG_PASSWORD_ONLY
#define EN_FATFS_SD         1
#else
#define EN_FATFS_SD         0
#endif
#if DEBUG_PASSWORD_ONLY
#define EN_AS608            1
#else
#define EN_AS608            0
#endif
#if DEBUG_PASSWORD_ONLY
#define EN_ESP8266_ONENET   DEBUG_PASSWORD_ESP8266_ENABLE
#else
#define EN_ESP8266_ONENET   0
#endif
#else /* 全功能 */
#undef EN_OV7670_LOCAL
#undef EN_FATFS_SD
#undef EN_AS608
#undef EN_ESP8266_ONENET
#define EN_OV7670_LOCAL     1       /* 本地 OV7670+FIFO（键盘 Y3/Y4 固定 PB1/PB2，与 PA0~PA7 数据口无共脚） */
#define EN_FATFS_SD         1
#define EN_AS608            1
#define EN_ESP8266_ONENET   1
#endif
#define EN_OV7670           EN_OV7670_LOCAL

/* SD 调试详细信息（E/R1/引脚电平/写阶段等）：1=显示，0=仅显示简洁 OK/ERR */
#ifndef SD_DEBUG_VERBOSE
#define SD_DEBUG_VERBOSE    1
#endif
/* C 键 SD 调试菜单（DEBUG_PASSWORD_ONLY）：当前默认关闭，避免在小栈环境下进入菜单触发 HardFault。 */
#ifndef ENABLE_SD_DEBUG_MENU
#define ENABLE_SD_DEBUG_MENU  0
#endif

/* 键盘杜邦线较长时电容/干扰大，易丢键：置 1 自动加大去抖与建立时间；根本办法仍是缩短线 + 列 Y 各并 4.7k～10k 到 3.3V */
#ifndef BOARD_KEY_MATRIX_LONG_WIRE
#define BOARD_KEY_MATRIX_LONG_WIRE  1
#endif
#if BOARD_KEY_MATRIX_LONG_WIRE
#ifndef KEY_MATRIX_DEBOUNCE_MS
#define KEY_MATRIX_DEBOUNCE_MS  8u
#endif
#ifndef KEY_MATRIX_SETTLE_US
#define KEY_MATRIX_SETTLE_US    220u
#endif
#ifndef KEY_MATRIX_READ_GAP_US
#define KEY_MATRIX_READ_GAP_US  60u
#endif
#endif

/* 矩阵键：按住达到该毫秒数后产生一次 Key_Scan；全功能与单测默认均约 1s，可按需 #define KEY_LONGPRESS_MS 覆盖 */
#ifndef KEY_LONGPRESS_MS
#define KEY_LONGPRESS_MS  90u
#endif
/* 连续匹配次数：2 比 3 更灵敏（代价是抗抖稍弱） */
#ifndef KEY_STABLE_MATCHES
#define KEY_STABLE_MATCHES  2u
#endif

/* ================= 1区：左排顶部（指纹 + 预留串口） ================= */
/* USART1：AS608 指纹 + 调试日志 */
#define BOARD_USART1_GPIO_CLK           RCC_APB2Periph_GPIOA
#define BOARD_USART1_TX_PORT            GPIOA                   /* PA9  */
#define BOARD_USART1_TX_PIN             GPIO_Pin_9
#define BOARD_USART1_RX_PORT            GPIOA                   /* PA10 */
#define BOARD_USART1_RX_PIN             GPIO_Pin_10

/* AS608 状态脚 PS_Sta */
#define BOARD_AS608_PS_STA_PORT         GPIOA                   /* PA8  */
#define BOARD_AS608_PS_STA_PIN          GPIO_Pin_8
#define BOARD_AS608_PS_STA_CLK          RCC_APB2Periph_GPIOA

/* AS608 识别速度：原 uart_recv 会傻等满超时(如400ms)才返回，改为“字节流静止”提前结束一帧 */
#ifndef AS608_UART_RECV_TIMEOUT_MS
#define AS608_UART_RECV_TIMEOUT_MS      150u  /* 单条指令兜底上限(ms)；正常通常 <50ms */
#endif
#ifndef AS608_FIND_IMG_GAP_MS
#define AS608_FIND_IMG_GAP_MS           12u   /* GetImage→GenChar 间隔，手册一般≥10ms */
#endif
#ifndef AS608_RX_IDLE_MS
#define AS608_RX_IDLE_MS                4u    /* 接收计数若干毫秒不增则认为一帧发完 */
#endif
#ifndef AS608_RX_MIN_BYTES
#define AS608_RX_MIN_BYTES              9u    /* 应答包至少含索引9的状态字节 */
#endif
/* 识别成功后的全屏提示：finger_ctrl 内 delay_ms(20) 循环次数 */
#ifndef AS608_UNLOCK_HOLD_LOOPS
#define AS608_UNLOCK_HOLD_LOOPS         100u  /* 100*20ms ~2s, was 250 ~5s */
#endif

/* USART3：ESP8266（AT/MQTT），与 PA0~PA7 摄像头并口无共脚 */
#define BOARD_USART3_APB1               RCC_APB1Periph_USART3
#define BOARD_USART3_GPIO_APB2          RCC_APB2Periph_GPIOB
#define BOARD_USART3_INSTANCE           USART3
#define BOARD_USART3_TX_PORT            GPIOB                   /* PB10 → 模块 RX */
#define BOARD_USART3_TX_PIN             GPIO_Pin_10
#define BOARD_USART3_RX_PORT            GPIOB                   /* PB11 ← 模块 TX */
#define BOARD_USART3_RX_PIN             GPIO_Pin_11

/* ================= 2区：左排中部（WiFi + 摄像头） ================= */
/* ESP8266 EN/CH_PD（避开 PA0~PA7 数据总线） */
#define BOARD_ESP8266_EN_PORT           GPIOC                   /* PC9  */
#define BOARD_ESP8266_EN_PIN            GPIO_Pin_9
#define BOARD_ESP8266_EN_CLK            RCC_APB2Periph_GPIOC
/* 1=使用 NodeMCU 开发板（不由 STM32 控制 EN）；
 * 0=使用裸 ESP 模块（含 ESP-01，STM32 控制 CH_PD/EN）
 */
#ifndef BOARD_WIFI_USE_NODEMCU
#define BOARD_WIFI_USE_NODEMCU          0
#endif
/* 1=WiFi 由上位机工具预配置并自动连接，STM32 不再下发 CWJAP；
 * 0=STM32 负责 CWJAP
 * 当前默认：ESP-01 常为新模块，默认让 STM32 直接按 WIFI_SSID/WIFI_PASS 联网。
 */
#ifndef BOARD_WIFI_PRECONFIGURED
#define BOARD_WIFI_PRECONFIGURED        0
#endif
/* ESP-01S 串口波特率（常见 AT 固件默认 115200，若你刷成 9600 可改这里） */
#ifndef BOARD_WIFI_UART_BAUD
#define BOARD_WIFI_UART_BAUD            115200u
#endif
/* WiFi STA 参数：调试 ESP-01S 时只需改这里，无需改业务代码 */
#ifndef BOARD_WIFI_SSID
#define BOARD_WIFI_SSID                 "Biye"
#endif
#ifndef BOARD_WIFI_PASS
#define BOARD_WIFI_PASS                 "12345678"
#endif
/* 1=打印 ESP-01S 基础 AT 信息（GMR/CWMODE/CWSTATE），便于联调定位 */
#ifndef BOARD_WIFI_DEBUG_BOOTINFO
#define BOARD_WIFI_DEBUG_BOOTINFO       1
#endif
/* 1=密码主界面 OLED 第0行不显示 W/N/S 联网摘要；调网时改为 0 即可恢复 */
#ifndef PASSWORD_OLED_HIDE_NET_LINE0
#define PASSWORD_OLED_HIDE_NET_LINE0    0
#endif

/* OV7670 + FIFO */
/* 数据总线 D0~D7：PA0~PA7 */
#define BOARD_OV_DATA_PORT              GPIOA

/* 时序控制 */
#define BOARD_OV_VSYNC_PORT             GPIOA                   /* PA11 */
#define BOARD_OV_VSYNC_PIN              GPIO_Pin_11
#define BOARD_OV_RCK_PORT               GPIOA                   /* PA12 */
#define BOARD_OV_RCK_PIN                GPIO_Pin_12

/* FIFO 控制 */
#define BOARD_OV_WRST_PORT              GPIOB                   /* PB3  */
#define BOARD_OV_WRST_PIN               GPIO_Pin_3
#define BOARD_OV_RRST_PORT              GPIOB                   /* PB4  */
#define BOARD_OV_RRST_PIN               GPIO_Pin_4
#define BOARD_OV_WREN_PORT              GPIOB                   /* PB5  */
#define BOARD_OV_WREN_PIN               GPIO_Pin_5
#define BOARD_OV_OE_PORT                GPIOB                   /* PB6  */
#define BOARD_OV_OE_PIN                 GPIO_Pin_6

/* SCCB：SCL 用 PD2，与 OV OE(PB6) 分离，避免同脚复用 */
#define BOARD_SCCB_SCL_PORT             GPIOD                   /* PD2  */
#define BOARD_SCCB_SCL_PIN              GPIO_Pin_2
#define BOARD_SCCB_SDA_PORT             GPIOB                   /* PB7  */
#define BOARD_SCCB_SDA_PIN              GPIO_Pin_7
#define BOARD_SCCB_GPIO_CLK             (RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOD)

/* ================= 3区：左排底部（人机 + SD） ================= */
/* 4x4 矩阵键盘（列选通扫描）：行 X 上拉输入、列 Y 推挽输出。R1~R4→X(PC0~3)，C1~C4→Y；R/C 反接会全无键 */
/* 杜邦线尽量 ≤20cm；长线配合 BOARD_KEY_MATRIX_LONG_WIRE=1 与列脚 4.7k～10k 上拉 */
/* 扫描时序：key4_4.c 默认；board_config 中 BOARD_KEY_MATRIX_LONG_WIRE 或自定义 KEY_MATRIX_* 可覆盖 */
#define BOARD_KEY_X1_PORT               GPIOC                   /* PC0  */
#define BOARD_KEY_X1_PIN                GPIO_Pin_0
#define BOARD_KEY_X1_RCC                RCC_APB2Periph_GPIOC

#define BOARD_KEY_X2_PORT               GPIOC                   /* PC1  */
#define BOARD_KEY_X2_PIN                GPIO_Pin_1
#define BOARD_KEY_X2_RCC                RCC_APB2Periph_GPIOC

#define BOARD_KEY_X3_PORT               GPIOC                   /* PC2  */
#define BOARD_KEY_X3_PIN                GPIO_Pin_2
#define BOARD_KEY_X3_RCC                RCC_APB2Periph_GPIOC

#define BOARD_KEY_X4_PORT               GPIOC                   /* PC3（勿接 PC10：蜂鸣器 BEEP；勿误接 PC13：不少核心板接 LED） */
#define BOARD_KEY_X4_PIN                GPIO_Pin_3
#define BOARD_KEY_X4_RCC                RCC_APB2Periph_GPIOC

#define BOARD_KEY_Y1_PORT               GPIOC                   /* PC11（核心板未引出 PC14 时接 Y1） */
#define BOARD_KEY_Y1_PIN                GPIO_Pin_11
#define BOARD_KEY_Y1_RCC                RCC_APB2Periph_GPIOC

#define BOARD_KEY_Y2_PORT               GPIOC                   /* PC12（核心板未引出 PC15 时接 Y2） */
#define BOARD_KEY_Y2_PIN                GPIO_Pin_12
#define BOARD_KEY_Y2_RCC                RCC_APB2Periph_GPIOC

/* 键盘列 Y3/Y4：固定 PB1/PB2（与 OV 并口 PA0~PA7 无共脚；勿改接 PA5/PA6） */
#define BOARD_KEY_Y3_PORT               GPIOB                   /* PB1  */
#define BOARD_KEY_Y3_PIN                GPIO_Pin_1
#define BOARD_KEY_Y3_RCC                RCC_APB2Periph_GPIOB

#define BOARD_KEY_Y4_PORT               GPIOB                   /* PB2  */
#define BOARD_KEY_Y4_PIN                GPIO_Pin_2
#define BOARD_KEY_Y4_RCC                RCC_APB2Periph_GPIOB

/* OLED 软件 I2C：PB8=SCL、PB9=SDA（SPI 七针屏不能当 I2C 用）
 * 新款 0.96" 四线 I2C 常标 SSD1315，排针顺序多为：GND → VDD(VCC) → SCK(SCL) → SDA（与旧款 VCC/GND 顺序可能不同，换屏时勿照搬旧杜邦线序）。
 * SSD1315 与 SSD1306 常用命令兼容，沿用 Driver/oled.c 的 OLED_Init；I2C 地址仍为 7-bit 0x3C(写 0x78) 或 0x3D(写 0x7A)，上电自动探测。
 * 接线：MCU PB8 → 屏 SCL/SCK，PB9 → 屏 SDA，3V3→VDD，GND→GND。 */
#define BOARD_OLED_GPIO_PORT            GPIOB
#define BOARD_OLED_PIN_SCL              GPIO_Pin_8              /* PB8 = SCL */
#define BOARD_OLED_PIN_SDA              GPIO_Pin_9              /* PB9 = SDA；仍无 ACK 可交换这两行定义试是否接反 */
#define BOARD_OLED_GPIO_CLK             RCC_APB2Periph_GPIOB
/* 默认在 oled.h 为 0x78(7位0x3C)；固件会在 OLED_Init 内自动尝试 0x78/0x7A。
 * 若仍不亮，先查杜邦：PB8=SCL、PB9=SDA、3V3、GND；模块须为 I2C 款而非 SPI 款。
 * 软件 I2C 位延时(μs)：杜邦线长/电容大时可略增(如 4)减轻花屏与误码；一般不会单独造成「稳定只缺首字母」，
 * 那种现象更常见是列偏移/可视区与 GRAM 不一致，见 OLED_COLUMN_OFFSET、OLED_GRAM_X_SHIFT。 */
#ifndef OLED_I2C_BIT_DELAY_US
#define OLED_I2C_BIT_DELAY_US         3u
#endif
/* 刷屏列起始（SSD1306 必须为 0；SH1106 常试 2）。
 * 误在 SSD1306 上设 2：从列 2 起写 128 字节会越界/折返，表现为首字整列丢、最左竖条噪点。
 * 若你的屏是 SH1106 且首列偏 2 像素，再在工程里改为 2。 */
/* OLED 屏型：
 * 0=SSD1306/SSD1315（列 0 起始）
 * 1=SH1106（常见需列偏移 2）
 */
#ifndef OLED_PANEL_TYPE
/* [M1.10] 从 0 改为 1：实物照片显示每行首字母被左切 (ET:INIT / nput Password /
 * ISARMED)，这是典型的 SH1106 面板被按 SSD1306 驱（列偏移 2 的可视区没对齐）
 * 的故障。OLED_COLUMN_OFFSET 会随之自动变为 2。 */
#define OLED_PANEL_TYPE              1u
#endif

#ifndef OLED_COLUMN_OFFSET
#if (OLED_PANEL_TYPE == 1u)
#define OLED_COLUMN_OFFSET            2u      /* SH1106 常见偏移 */
#else
#define OLED_COLUMN_OFFSET            0u      /* SSD1306/SSD1315 */
#endif
#endif
/* 列地址低字节 OR 掩码（仅低 4 位）：SSD1306/SSD1315 维持 0，避免第 0 列被意外跳过 */
#ifndef OLED_COLUMN_ADDR_LOW_OR
#define OLED_COLUMN_ADDR_LOW_OR       0u
#endif
/* 逻辑坐标相对 GRAM 右移列数：默认 0；若最左一整格被塑料挡掉可试 8u */
#ifndef OLED_GRAM_X_SHIFT
#define OLED_GRAM_X_SHIFT             0u
#endif

/* 上电是否自动显示 OLED 屏型/首列自检图案 */
#ifndef OLED_POWERON_SELFTEST_ENABLE
#define OLED_POWERON_SELFTEST_ENABLE  1u
#endif
#ifndef OLED_POWERON_SELFTEST_MS
#define OLED_POWERON_SELFTEST_MS      1200u
#endif

/* SD 卡 SPI2：PB13/PB14/PB15 + CS=PB12 */
#define BOARD_SD_CS_PORT                GPIOB                   /* PB12 */
#define BOARD_SD_CS_PIN                 GPIO_Pin_12
#define BOARD_SD_CS_CLK                 RCC_APB2Periph_GPIOB

/*
 * SD SPI 保守初始化（慢速、易兼容差卡/杜邦线长）；新卡到手后可略收紧以缩短启动时间。
 * SD_SPI_PLACEHOLDER_SECTORS：未读 CSD 前占位容量（约 4GiB = 4*1024*1024*1024/512）。
 */
#ifndef SD_SPI_INIT_POWERUP_MS
#define SD_SPI_INIT_POWERUP_MS          12u     /* CS 高后等待上电/电平稳定 */
#endif
#ifndef SD_SPI_INIT_DUMMY_BYTES
#define SD_SPI_INIT_DUMMY_BYTES         80u     /* >= 74 个时钟，给卡进入 SPI 模式 */
#endif
#ifndef SD_SPI_CMD_WAIT_READY_MS
#define SD_SPI_CMD_WAIT_READY_MS        200u    /* 发命令前总线空闲等待（原 50） */
#endif
#ifndef SD_SPI_SELECT_RETRY
#define SD_SPI_SELECT_RETRY             20000u  /* sd_select 内约等价毫秒级重试 */
#endif
#ifndef SD_SPI_ACMD41_MAX_RETRIES
#define SD_SPI_ACMD41_MAX_RETRIES       600u    /* ACMD41 轮询上限（次） */
#endif
#ifndef SD_SPI_ACMD41_RETRY_DELAY_MS
#define SD_SPI_ACMD41_RETRY_DELAY_MS    2u      /* 每次轮询间隔，降低总线压力 */
#endif
#ifndef SD_SPI_PLACEHOLDER_SECTORS
#define SD_SPI_PLACEHOLDER_SECTORS      8388608u /* 4GiB @ 512B/sector，匹配 4GB 级 SDHC */
#endif

/* ================= 4区：右排（传感器 + 执行器） ================= */
/* PIR 人体红外 */
#define BOARD_PIR_GPIO_PORT             GPIOC                   /* PC4  */
#define BOARD_PIR_GPIO_PIN              GPIO_Pin_4
#define BOARD_PIR_GPIO_CLK              RCC_APB2Periph_GPIOC
#define BOARD_PIR_GPIO_PORT_SOURCE      GPIO_PortSourceGPIOC
#define BOARD_PIR_GPIO_PIN_SOURCE       GPIO_PinSource4
#define BOARD_PIR_EXTI_LINE             EXTI_Line4
#define BOARD_PIR_EXTI_IRQ              EXTI4_IRQn

/* DHT 单总线（DHT11 / DHT22 共用此引脚） */
#define BOARD_DHT11_GPIO_PORT           GPIOC                   /* PC5  */
#define BOARD_DHT11_GPIO_PIN            GPIO_Pin_5
#define BOARD_DHT11_GPIO_CLK            RCC_APB2Periph_GPIOC
/* OLED 诊断页标签，与上面引脚保持一致 */
#ifndef BOARD_DHT_PIN_LABEL_STR
#define BOARD_DHT_PIN_LABEL_STR         "PC5"
#endif
/* 11=DHT11，22=DHT22（当前改为 DHT11） */
#ifndef BOARD_DHT_SENSOR_TYPE
#define BOARD_DHT_SENSOR_TYPE           11
#endif
/* DHT 单总线时序（参考程序风格：保持原始时序、降低干扰） */
#ifndef BOARD_DHT_WAIT_TIMEOUT_US
#define BOARD_DHT_WAIT_TIMEOUT_US       500u    /* 等待电平跳变超时 */
#endif
#ifndef BOARD_DHT_RST_LOW_MS
#define BOARD_DHT_RST_LOW_MS            20u     /* 起始拉低 >=18ms */
#endif
#ifndef BOARD_DHT_RST_RELEASE_US
#define BOARD_DHT_RST_RELEASE_US        40u     /* 释放后 20~40us */
#endif
#ifndef BOARD_DHT_BIT_SAMPLE_US
#define BOARD_DHT_BIT_SAMPLE_US         40u     /* 更接近 DHT11 经典采样点 */
#endif
/* DHT11 单次读失败时内部快速重试次数 */
#ifndef BOARD_DHT11_INTERNAL_RETRY
#define BOARD_DHT11_INTERNAL_RETRY      5u
#endif

/* MQ-2 ADC */
#define BOARD_MQ2_ADC_GPIO_PORT         GPIOB                   /* PB0  */
#define BOARD_MQ2_ADC_GPIO_PIN          GPIO_Pin_0
#define BOARD_MQ2_ADC_GPIO_CLK          RCC_APB2Periph_GPIOB
#define BOARD_MQ2_ADC_CHANNEL           ADC_Channel_8
#define BOARD_MQ2_ADC_SAMPLE_TIME       ADC_SampleTime_239Cycles5
/* 1=启用燃气超限蜂鸣+关阀；PB0 未接 MQ2 或杜邦线悬空时 ADC 易偏高，预热约 25s 后会误报，调通硬件后改为 1 */
/* [M1] 全功能默认 1；若 PB0 悬空要临时屏蔽误报，上方 #define BOARD_MQ2_ALARM_ENABLE 0 即可覆盖。 */
#ifndef BOARD_MQ2_ALARM_ENABLE
#define BOARD_MQ2_ALARM_ENABLE          1
#endif

/* 密码调试模式默认关闭 MQ-2，避免 PB0 悬空导致上电即误报；要联调 MQ2 时再手动改回 1 */
#if DEBUG_PASSWORD_ONLY
#undef BOARD_MQ2_ALARM_ENABLE
#define BOARD_MQ2_ALARM_ENABLE          0
#endif

/* 执行器 */
#define BOARD_DOOR_LOCK_PORT            GPIOC                   /* PC6  门锁继电器（gpio.h 中 RELAY=PCout(6) 须与此一致；旧版误用 PB12 已纠正） */
#define BOARD_DOOR_LOCK_PIN             GPIO_Pin_6
#define BOARD_DOOR_LOCK_CLK             RCC_APB2Periph_GPIOC

#define BOARD_GAS_VALVE_RELAY_PORT      GPIOC                   /* PC7  燃气阀继电器 */
#define BOARD_GAS_VALVE_RELAY_PIN       GPIO_Pin_7
#define BOARD_GAS_VALVE_RELAY_CLK       RCC_APB2Periph_GPIOC

#define BOARD_LIGHT_RELAY_PORT          GPIOC                   /* PC8  照明继电器 */
#define BOARD_LIGHT_RELAY_PIN           GPIO_Pin_8
#define BOARD_LIGHT_RELAY_CLK           RCC_APB2Periph_GPIOC

/* 1：printf→fputc 走 PC8 软件串口 TX（默认 57600），USB‑TTL 的 RX 接 PC8、GND 共板；PA9/PA10 专供指纹 USART1 */
/* 0：printf 走 USART1 PA9；PC8 由 Linkage 作照明继电器 */
#ifndef BOARD_PRINTF_SOFTUART_PC8
#define BOARD_PRINTF_SOFTUART_PC8       0
#endif
#ifndef BOARD_SOFTUART_BAUD
#define BOARD_SOFTUART_BAUD             57600u
#endif

/* 蜂鸣器：与 gpio.h 中 BEEP_SoundOn/Off、gpio.c 一致；勿与键盘行线共用 */
/* 0=低电平触发（IO 拉低响，默认，与本工程有源蜂鸣器一致）；1=高电平触发。勿混用 */
#ifndef BOARD_BEEP_ACTIVE_HIGH
#define BOARD_BEEP_ACTIVE_HIGH          0
#endif
#define BOARD_BEEP_PORT                 GPIOC
#define BOARD_BEEP_PIN                  GPIO_Pin_10
#define BOARD_BEEP_CLK                  RCC_APB2Periph_GPIOC

/* =============================================================================
 * [M1 增补] 后续模块预留宏（M2 IWDG / M3 FreeRTOS 任务 / M4~M9 算法参数）
 * -----------------------------------------------------------------------------
 * 说明：本段宏在 M1 阶段"只声明不使用"，后续 M2/M3/... 模块落地时再引用生效。
 * 保持 #ifndef 保护，允许各模块 .c 内临时覆盖。不改变任何现有代码行为。
 * =============================================================================
 */

/* ---------- M2 IWDG 独立看门狗 ---------- */
/* 1=启用 IWDG 硬件看门狗；0=关闭（仅调试期或 JTAG 断电烧录时临时关） */
#ifndef BOARD_IWDG_ENABLE
#define BOARD_IWDG_ENABLE               1
#endif
/* IWDG 复位周期(ms)：LSI 40kHz、预分频 64、Reload≈1250 → 约 2000ms */
#ifndef BOARD_IWDG_TIMEOUT_MS
#define BOARD_IWDG_TIMEOUT_MS           2000u
#endif
/* 需要按时喂狗的"活标志位"源数量（M3 系统监测任务聚合后统一 IWDG_Feed） */
#ifndef BOARD_IWDG_WATCH_SRC_MAX
#define BOARD_IWDG_WATCH_SRC_MAX        8u
#endif

/* ---------- M3 FreeRTOS 任务表（论文表 5-x 对齐） ---------- */
/* 任务优先级：数值越大越高；上限为 FreeRTOSConfig 的 configMAX_PRIORITIES(=8) - 1 = 7 */
#ifndef APP_TASK_PRIO_SECURITY
#define APP_TASK_PRIO_SECURITY          5u      /* 安防告警（硬实时，事件+周期） */
#endif
#ifndef APP_TASK_PRIO_NET
#define APP_TASK_PRIO_NET               4u      /* 网络通信 */
#endif
#ifndef APP_TASK_PRIO_FINGER
#define APP_TASK_PRIO_FINGER            3u      /* 指纹识别 */
#endif
#ifndef APP_TASK_PRIO_SENSOR
#define APP_TASK_PRIO_SENSOR            3u      /* 传感采集 */
#endif
#ifndef APP_TASK_PRIO_CAPTURE
#define APP_TASK_PRIO_CAPTURE           2u      /* 图像抓拍 */
#endif
#ifndef APP_TASK_PRIO_SYSMON
#define APP_TASK_PRIO_SYSMON            1u      /* 系统监测 */
#endif

/* 任务栈深(FreeRTOS StackDepth 单位=Word=4 字节) */
#ifndef APP_TASK_STACK_SECURITY
#define APP_TASK_STACK_SECURITY         512u
#endif
#ifndef APP_TASK_STACK_NET
#define APP_TASK_STACK_NET              640u
#endif
#ifndef APP_TASK_STACK_FINGER
#define APP_TASK_STACK_FINGER           512u
#endif
#ifndef APP_TASK_STACK_SENSOR
#define APP_TASK_STACK_SENSOR           384u
#endif
#ifndef APP_TASK_STACK_CAPTURE
#define APP_TASK_STACK_CAPTURE          768u
#endif
#ifndef APP_TASK_STACK_SYSMON
#define APP_TASK_STACK_SYSMON           320u
#endif

/* 任务周期(ms)；事件触发型任务 = 0 表示仅阻塞等事件 */
#ifndef APP_TASK_PERIOD_SECURITY_MS
#define APP_TASK_PERIOD_SECURITY_MS     50u
#endif
#ifndef APP_TASK_PERIOD_NET_MS
#define APP_TASK_PERIOD_NET_MS          100u
#endif
#ifndef APP_TASK_PERIOD_FINGER_MS
#define APP_TASK_PERIOD_FINGER_MS       50u
#endif
#ifndef APP_TASK_PERIOD_SENSOR_MS
#define APP_TASK_PERIOD_SENSOR_MS       200u
#endif
#ifndef APP_TASK_PERIOD_SYSMON_MS
#define APP_TASK_PERIOD_SYSMON_MS       2000u
#endif

/* 任务间通信深度（M3 按需创建：队列/事件组） */
#ifndef APP_IPC_ALARM_Q_DEPTH
#define APP_IPC_ALARM_Q_DEPTH           8u
#endif
#ifndef APP_IPC_CAP_Q_DEPTH
#define APP_IPC_CAP_Q_DEPTH             4u
#endif
#ifndef APP_IPC_CMD_Q_DEPTH
#define APP_IPC_CMD_Q_DEPTH             6u
#endif

/* ---------- M4 PIR 三段式误触发抑制（毛刺 + 最小持续 + 锁定） ---------- */
#ifndef APP_PIR_GLITCH_REJECT_MS
#define APP_PIR_GLITCH_REJECT_MS        60u     /* < 60ms 窄脉冲直接丢弃 */
#endif
#ifndef APP_PIR_MIN_ACTIVE_MS
#define APP_PIR_MIN_ACTIVE_MS           200u    /* ≥ 200ms 持续才视为真触发 */
#endif
#ifndef APP_PIR_LOCKOUT_MS
#define APP_PIR_LOCKOUT_MS              3000u   /* 单次触发后锁定窗口（复用原 SUPPRESS） */
#endif

/* ---------- M5 DHT11/22 最小二乘法校准系数（论文 §3-2 固化 Q15 定点） ---------- */
#ifndef APP_DHT_CALIB_ENABLE
#define APP_DHT_CALIB_ENABLE            1
#endif
#ifndef APP_DHT_CALIB_AT_Q15
#define APP_DHT_CALIB_AT_Q15            32148   /* round(0.9812 * 32768) */
#endif
#ifndef APP_DHT_CALIB_BT_Q15
#define APP_DHT_CALIB_BT_Q15            9982    /* round(0.3046 * 32768) */
#endif
#ifndef APP_DHT_CALIB_AH_Q15
#define APP_DHT_CALIB_AH_Q15            32112   /* round(0.9800 * 32768) */
#endif
#ifndef APP_DHT_CALIB_BH_Q15
#define APP_DHT_CALIB_BH_Q15            13180   /* round(0.4022 * 32768) */
#endif

/* ---------- M6 MQ-2 中值 + 滑动平均级联 + 温漂补偿 + 变化率双判据 ---------- */
#ifndef APP_MQ2_MEDIAN_WIN
#define APP_MQ2_MEDIAN_WIN              5u      /* 中值窗口（奇数） */
#endif
#ifndef APP_MQ2_MA_WIN
#define APP_MQ2_MA_WIN                  8u      /* 滑动平均窗口 */
#endif
/* 温漂补偿：ADC_out = ADC_raw - APP_MQ2_TEMP_K_Q8 * (T - 25) / 256 */
#ifndef APP_MQ2_TEMP_K_Q8
#define APP_MQ2_TEMP_K_Q8               12      /* 温漂系数（Q8 定点） */
#endif
#ifndef APP_MQ2_TEMP_BASE_C
#define APP_MQ2_TEMP_BASE_C             25
#endif
/* 变化率（斜率）判据：ΔADC/周期 超过阈值立即告警 */
#ifndef APP_MQ2_SLOPE_THRESHOLD
#define APP_MQ2_SLOPE_THRESHOLD         120u
#endif

/* ---------- M7 门禁五态 FSM + AS608 指纹识别任务 ---------- */
#ifndef APP_LOCK_MAX_FAIL
#define APP_LOCK_MAX_FAIL               3u      /* 连续失败锁定阈值 */
#endif
#ifndef APP_LOCK_LOCKOUT_MS
#define APP_LOCK_LOCKOUT_MS             30000u  /* 锁定持续时长 */
#endif
#ifndef APP_LOCK_OPEN_HOLD_MS
#define APP_LOCK_OPEN_HOLD_MS           3000u   /* 开锁保持时长（继电器吸合） */
#endif
#ifndef APP_FINGER_MATCH_SCORE_MIN
#define APP_FINGER_MATCH_SCORE_MIN      50u     /* AS608 PS_Search 置信度下限 */
#endif

/* ---------- M8 多传感器融合证据加权评分 ---------- */
#ifndef APP_FUSION_W_PIR
#define APP_FUSION_W_PIR                60u
#endif
#ifndef APP_FUSION_W_MQ2
#define APP_FUSION_W_MQ2                70u
#endif
#ifndef APP_FUSION_W_CAM_MOTION
#define APP_FUSION_W_CAM_MOTION         40u
#endif
#ifndef APP_FUSION_TRIGGER_SCORE
#define APP_FUSION_TRIGGER_SCORE        80u     /* 触发告警联动的评分阈值 */
#endif

/* 场景模式 ID（M8 scene.c 使用） */
#ifndef SCENE_MODE_HOME
#define SCENE_MODE_HOME                 0u      /* 居家：不布防、灯随手开 */
#endif
#ifndef SCENE_MODE_AWAY
#define SCENE_MODE_AWAY                 1u      /* 离家布防：PIR 触发立告警 */
#endif
#ifndef SCENE_MODE_NIGHT
#define SCENE_MODE_NIGHT                2u      /* 夜间安防：部分区域 PIR 生效 */
#endif

/* ---------- M9 MQTT 命令幂等去重 + AES-128 ---------- */
#ifndef APP_CMD_DEDUP_RING
#define APP_CMD_DEDUP_RING              16u     /* 去重环深度（近 16 条 cmdId） */
#endif
#ifndef APP_CMD_STALE_MS
#define APP_CMD_STALE_MS                60000u  /* 超过该时效的命令直接丢弃 */
#endif
#ifndef APP_SECURE_AES_ENABLE
#define APP_SECURE_AES_ENABLE           1       /* admin 密码等敏感数据 AES-128 落盘 */
#endif

#endif /* __BOARD_CONFIG_H */
