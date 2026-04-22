# 豆包 AI / 大模型用 · 工程全量知识库（STM32 单芯片智能安防·已去双机）

> **用途**：为豆包等对话模型提供**单文件入口**，覆盖硬件/固件路径、引脚分配、任务架构、OneNET、抓拍、排障与文档索引，减少“主从/双机”过时描述与幻觉。  
> **事实优先级**：**源码 > `User\board_config.h` > 其它 Markdown**。  
> **工程根**（相对路径）：`biye\`；固件代码在 **`biye\代码\`**。  
> **重要变更**：原 `Slave_F103/`、`slave_comm.*`、`comm_proto.h` 等**双机通信已彻底删除**，工程为**单 MCU**全功能方案。

---

## 1. 项目一句话

单颗 **STM32F103RCT6（LQFP64，大容量）** 跑 **FreeRTOS + STD 外设库**：门禁（密码/指纹）、传感器、继电器联动、**OV7670+FIFO 本地抓拍写 SD(BMP)**、**ESP8266（USART3 PB10/PB11，EN=PC9）OneNET（AT+MQTT）**、日志与安全存储。

---

## 2. 工程路径与 Keil

| 内容 | 目录/文件 |
|------|-----------|
| 工程目录 | `biye\代码\` |
| Keil 工程 | `biye\代码\User\程序.uvprojx` |
| 目标芯片 | **STM32F103RCT6（建议用 HD 启动文件）** |

> 说明：仓库同时包含 `startup_stm32f10x_md.s` 与 `startup_stm32f10x_hd.s`；单芯片 RCT6 建议选择 **HD** 对应的启动文件与芯片容量配置。

---

## 3. 串口拓扑（单芯片固定分配）

| 链路 | 引脚 | 波特率（以 `User\main.c` `App_RTOS_Init()` 为准） | 用途 |
|------|------|-----------------------------------------------|------|
| **USART1** | PA9(TX)/PA10(RX) | 57600（常用） | AS608 指纹 + printf/日志（共用） |
| **USART3** | PB10(TX)/PB11(RX) | 115200 | **ESP8266 + OneNET（AT+MQTT）**；由 **`USART2_Init_Config(115200)`** 初始化（函数名历史遗留） |
| **USART2** | PA2/PA3（默认复用） | — | **本工程未初始化 USART2**；**PA2~PA4** 归入 **OV7670 并口 D2~D4**，勿接 WiFi |

铁律：
- **USART1 固定给指纹/调试**；**ESP8266 仅接 USART3（PB10/PB11）**，与 **PA0~PA7 摄像头并口无共脚**。
- 工程不再存在 `comm_proto.h`、`slave_comm.*`，**无主从串口协议**。

---

## 4. 单芯片引脚全表（权威：`User\board_config.h`，与 `硬件\硬件连接方案.md` 一致）

> **禁止**使用本节旧版或其它文档中已废止的分配（如键盘占 PB5–PB7、SD_CS=PA4、PS_Sta=PC14、EN=PC3 等）；**一律以下表为准**。

| 功能 | 引脚 | 备注 |
|------|------|------|
| USART1 TX/RX | PA9 / PA10 | AS608 + `printf`，57600 |
| USART3 TX/RX | **PB10 / PB11** | ESP8266，115200（`USART2_Init_Config` 内初始化 USART3） |
| ESP8266 EN/CH_PD | **PC9** | GPIO；与 OV 并口、SD_CS 无共脚 |
| AS608 PS_Sta | **PA8** | 状态输入 |
| USART2（PA2/PA3） | — | 未用于 ESP；**PA2、PA3** 为 **OV D2、D3** |
| OLED SCL/SDA（软件 I2C） | PB8 / PB9 | SSD1306 |
| SD SPI2 | **PB12(CS)、PB13(SCK)、PB14(MISO)、PB15(MOSI)** | FatFs **`0:`** |
| 矩阵键盘行 X1–X4 | **PC0、PC1、PC2、PC3** | 推挽输出；**X4 用 PC3**，与 **PC10 蜂鸣器（`BEEP`）** 分离 |
| 矩阵键盘列 Y1–Y2 | **PC11、PC12** | 上拉输入，低有效（核心板 **PC14/PC15 未引出** 时的接线） |
| 矩阵键盘列 **Y3–Y4** | **默认 `EN_OV7670_LOCAL=1`：PB1、PB2**；**`=0`：PA5、PA6**（不接摄像头调试） | 开本地摄像头须 **PB1/PB2**，避免与 OV 数据 **D5/D6** 共脚 |
| OV7670 数据 D0–D7 | PA0–PA7 | **默认** `EN_OV7670_LOCAL=1` 时使用；`=0` 时可用于其它功能（勿与键盘 Y3/Y4 的 PA5/6 方案混接） |
| OV VSYNC / RCK | PA11 / PA12 | FIFO 读时钟在 PA12 |
| FIFO WRST/RRST/WREN/OE | PB3 / PB4 / PB5 / PB6 | PB6 **仅** FIFO OE |
| SCCB SCL/SDA | **PD2 / PB7** | 与 PB6(OE) 分离 |
| PIR | **PC4** | EXTI4（`board_config` 中 `EXTI_Line4`） |
| DHT11 | **PC5** | 单总线 |
| MQ-2 AO | **PB0** | ADC1_IN8 |
| 门锁继电器 | **PC6** | |
| 燃气阀继电器 | **PC7** | 应急关阀等 |
| 联动照明继电器 | **PC8** | `linkage` |
| 蜂鸣器 | **PC10** | **`gpio.h`**：`BEEP_SoundOn`/`BEEP_SoundOff`；**`board_config.h`**：`BOARD_BEEP_*`、`BOARD_BEEP_ACTIVE_HIGH`（极性）；**`gpio.c`** 初始化 |
| 通风/空调继电器 | — | **本版固件不包含** |
| SWD | PA13 / PA14 | 关 JTAG、**仅保留 SWD**；勿作 GPIO |

**宏开关（节选）**：`EN_OV7670_LOCAL`（默认 **1**，本地 OV 抓拍）、`EN_FATFS_SD`、`EN_AS608`、`EN_ESP8266_ONENET`。

**杜邦线分束**：人类接线优先看 **`硬件\硬件连接方案.md` §2**（按模块成束）。

---

## 5. 摄像头/抓拍（本地 OV7670+FIFO）

权威文件：
- 初始化与 FIFO 读：`Driver\ov7670_fifo.c/.h`（`ov_gpio_init` 已开 **GPIOD** 时钟供 **PD2** SCCB）
- SCCB：`Driver\sccb.c/.h`（引脚来自 `board_config.h`）
- 采集与任务：`User\Capture.c`（`Task_Capture`）、`User\capture_task.c`（`g_cap_q`）

要点：
- **`EN_OV7670_LOCAL=1`（默认）**：`App_RTOS_Init` 调用 **`OV7670_Init`**；`Task_Capture` 消费 **`g_cap_q`** 并写 **SD(BMP)**；键盘 **Y3/Y4** 为 **PB1/PB2**（见 §4）。**PIR=PC4、DHT11=PC5** 与 **PA0~PA7** 无脚位冲突。
- **`EN_OV7670_LOCAL=0`**（不接摄像头调试）：不初始化 OV；`Task_Capture` **占位延时**，不写盘；键盘 **Y3/Y4** 为 **PA5/PA6**；`Capture_Request` 入队可能不被消费。
- 变量 **`g_capture_busy`** 在源码中仅定义，**未见**与 PIR/传感器联动的分时逻辑；排障时勿假设已实现「抓拍时关 EXTI」等行为。

---

## 6. 抓拍文件输出

- FATFS 卷：`0:`（SD 根目录）
- 文件名：`CAP_<Tick>.BMP`
- 格式：BMP + RGB565 + **倒序存储（Bottom-up）**（见 `User\Capture.c`）

---

## 7. 双机通信/协议（已删除）

工程已无以下内容（**禁止再描述为存在**）：
- `comm_proto.h` / `comm.h`
- `slave_comm.c/.h`、`task_slave_comm`
- “主从串口帧”、“USART3 分包收图”等

---

## 8. 初始化摘要（`User\main.c`）

- `GPIO_Remap_SWJ_JTAGDisable`：关闭 JTAG，仅保留 SWD（释放 **PA15、PB3、PB4** 作普通 GPIO；**PB3/PB4** 在接 OV 时用作 FIFO **WRST/RRST**）
- `USART2_Init_Config(115200)`：**实际初始化 USART3（PB10/11）供 ESP8266**；`USART3_Init_Config` 为空实现（避免重复初始化 HardFault）  
- `uart1_Init(57600)`：AS608 + 日志  
- `OLED_Init()`、`MQ2_Init()`、`SdCapacity_Init()`；**默认** **`OV7670_Init()`**（`EN_OV7670_LOCAL=1`）；`=0` 时不调用

---

## 9. FreeRTOS 任务（`App_RTOS_Create`，见 `User\main.c`）

| 任务 | 职责摘要 |
|------|----------|
| `Task_Security_Alarm` | 安防轮询、`Security_Alarm_Run`、MQ-2 紧急、消警 |
| `Task_Wifi_MQTT` | **OneNET 全链路**：`ESP8266_OneNET_Full_Init`（失败重试）、**仅 `tls_inited==1` 时** `OneNET_Parse_Cmd` → 平台控制（`Door_Open`/`Arm_*`/`Led_Ctrl`）→ `OneNET_Publish_Data`；末尾 `UART_Remote_Ctrl`、`Security_Check_Serial_Command`；**`vTaskDelay(2000)`** |
| `Task_Fingerprint` | AS608 |
| `Task_KeyScan` | 矩阵键盘 |
| `Task_Sensor` | DHT11（**PC5**）、MQ-2（**PB0**）、PIR（**PC4**）等 |
| `Task_Actuator` | 门锁、蜂鸣器、阀门（队列） |
| `Task_OLED` | OLED 三行仪表盘：Gas/T&H/Status（无主从串口 OLED 指令） |
| `Task_SystemMonitor` | 约 **2 s**：**仅** `DeviceStatus_PrintMqttJson()`（USART1）；**OneNET 上报已在 `Task_Wifi_MQTT`** |
| `Task_LinkageCmd` | USART1 `CMD:`、联动与日志命令解析 |
| `Task_Capture` | **默认**：从 `g_cap_q` 取事件，OV7670+FIFO → SD(BMP)；**`EN_OV7670_LOCAL=0`**：占位延时，不写盘 |

另有：`Capture_Task_Create()`（创建抓拍事件队列 `g_cap_q`）、`Capture_Request()`（触发抓拍）。

### 9.1 `Task_Wifi_MQTT` 与 OneNET（与 `main.c` / `esp8266_tls.c` 一致）

- **`tls_inited`**：全局变量，定义于 **`esp8266_tls.c`**，头文件 **`esp8266_tls.h`** 中 `extern`；**仅**在 **`ESP8266_OneNET_Full_Init()`** 内，当 **WiFi（`ESP8266_TLS_Init`）+ `ESP8266_OneNET_Mqtt_Connect()`** 均成功时置 **1**，否则保持 **0**；失败时 **UART1** 打印 `[OneNET] ...` 原因。  
- **`ESP8266_Online_Flag`**：在 **`esp8266_onenet_mqtt.c`** 的 MQTT 连接成功后置 **1**；**`OneNET_Publish_Data`** 内仍会判断。  
- **WiFi 宏**（`esp8266_tls.c`）：**SSID `vivo S50`**，**密码 `12345678`**（2.4G AP）。  
- **平台宏**（`esp8266_onenet_mqtt.h`）：**`ONENET_PRODUCT_ID`**、**`ONENET_DEVICE_NAME`**、**`ONENET_MQTT_PASSWORD`**、**Broker `mqtts.heclouds.com:8883`**、**`ONENET_MQTT_SCHEME_SSL=2`**。  
- **下行解析**：**`OneNET_Parse_Cmd`** 更新 **`Ctrl_Led` / `Ctrl_Door` / `Ctrl_Arm`**（`esp8266_tls.c`）。  
- **本地执行**（`main.c` 静态函数）：**`Ctrl_Door==1`** → **`Door_Open()`** 后 **`Ctrl_Door=0`**；**`Ctrl_Arm`** → **`Arm_System_Enable` / `Arm_System_Disable`**；**`Led_Ctrl(Ctrl_Led)`** → **`Light_Relay_On/Off`**（`linkage`）。  
- **`main.c` 仅 `#include "esp8266_tls.h"`** 即可声明 **`ESP8266_OneNET_Full_Init`、`tls_inited`、`OneNET_*`**（实现分散在 `esp8266_tls.c` / `esp8266_onenet_mqtt.c`）。

---

## 10. 周期 JSON（APP 调试）

- **函数**：`DeviceStatus_PrintMqttJson()`（`sd_capacity.c`），**`Task_SystemMonitor`** 约 **2 s**。  
- **输出**：**USART1 `printf`**，`\r\n` 结尾。  
- **字段**：`temp`、`humi`、`gas`、`door`、`arm`、`sd`（嵌套）等；**`sd.status` 占位时可为 `unmounted`**。  
- **监听**：USB 转 TTL 接 **PA9/10**，波特率 **同 `uart1_Init`**。  
- **与 OneNET**：**物模型 JSON 上报**走 **USART3（ESP8266）** 的 **`OneNET_Publish_Data` → `AT+MQTTPUBRAW`**（在 **`Task_Wifi_MQTT`**）；软件缓冲符号仍为 **`Uart2_Buf` / `Find2` / `CLR_Buf2`**，**与 USART1 JSON 格式、通道均不同**。

---

## 11. 串口文本（USART1）

- **日志查询**：前缀 **`CMD:`**，如 `CMD:LOG=LAST`、`CMD:LOG=LAST,10`。

---

## 12. ESP8266 / OneNET（当前架构，2026 同步源码）

| 文件 | 作用 |
|------|------|
| **`User\esp8266_tls.h`** | 声明 **`ESP8266_TLS_Init`、`ESP8266_OneNET_Full_Init`、`ESP8266_AT_SendWait`、`OneNET_Parse_Cmd`、`OneNET_Publish_Data`、`ESP8266_OneNET_Mqtt_Connect`**；**`extern tls_inited`、`Ctrl_*`、`ESP8266_Online_Flag`**。**无** `MQTT_TLS_ENABLE` / `ESP8266_TLS_Connect_MQTT` 裸 SSL 双路径。 |
| **`User\esp8266_tls.c`** | **WiFi**（`AT+CWMODE`、`AT+CWJAP`，宏 **vivo S50 / 12345678**）、**`ESP8266_TLS_Init`**；**`ESP8266_OneNET_Full_Init`** 串联 **TLS_Init + OneNET_Mqtt_Connect** 并维护 **`tls_inited`**；**`OneNET_Publish_Data`** 组 OneJson 后调 **`OneNET_AT_Mqtt_PublishRaw`**；**`OneNET_Parse_Cmd`**。 |
| **`User\esp8266_onenet_mqtt.c/.h`** | **`ESP8266_OneNET_Mqtt_Connect`**（`AT+MQTTUSERCFG`、`AT+MQTTCONN`）；**`OneNET_AT_Mqtt_PublishRaw`**（`AT+MQTTPUBRAW`）；**`ONENET_MQTT_ENABLE`**。 |

**模块要求**：ESP8266 固件须支持 **Espressif AT+MQTT** 指令集；否则需换固件或关闭 **`ONENET_MQTT_ENABLE`** 并自行改连云方案。

**人类可读长文**：`硬件\ESP8266说明.md`（若与源码冲突以源码为准，文档可能滞后时需对照本节修订）。

---

## 13. 串口接收与缓冲（`main.c`）

- **`Uart2_Buf` / `Uart3_Buf`**：**`USART3_IRQHandler`**（ESP8266 所在 **USART3**）对每字节 **同源双写**：**`Uart2_Buf`**（**`Buf2_Max=768`**）供 **`Find2`/`CLR_Buf2`/OneNET**；**`Uart3_Buf`**（**`Buf3_Max=100`**）供 **`UART_Remote_Ctrl`/`Security_Check_Serial_Command`** 等。  
- **`USART2_IRQHandler`**：若 **USART2** 未使能则通常不进入；**勿**将 ESP 接到 PA2/PA3。  
- **`CLR_Buf2` / `CLR_Buf`**：清零缓冲并重置写索引。  
- **设计注意**：**`strstr`/`OneNET_Parse_Cmd`** 依赖 C 字符串；缓冲未满且未回绕时，IRQ 内在下一字节位置写 **`'\0'`** 以降低未终止字符串风险；**回绕后**仍可能不适合当作单段字符串解析。

---

## 14. `stm32f10x_it.c` 与 FreeRTOS

- **SVC / PendSV / SysTick**：由 **FreeRTOS `port.c`** 提供；本文件内对应 handler **注释掉**，避免重复定义。  
- **Fault 类**：默认 **死循环**；调试时可接调试器分析。

---

## 15. 首次上电检查（摘要）

1. 各模块与 MCU **GND 共地**（USB-TTL 调试须接 GND）。  
2. **无主从 USART 交叉线**；**USART3（PB10/11）接 ESP8266**，**不得**再接第二颗 MCU 作历史主从。  
3. ESP8266：**PB10→模块 RX，PB11←模块 TX**，**115200**；**EN→PC9**（以 `board_config.h` 为准）；**勿占用 PA0~PA7 作串口**。  
4. **勿把 ESP8266 接 USART1**（与指纹 / `printf` 冲突）；USART1 默认 **57600**。  
5. OLED **PB8/PB9**；SD **PB12(CS)+PB13–15**；AS608 **PA9/10 + PA8(PS_Sta)**。  
6. 键盘：行 **PC0/1/2/13**，列 **Y1/Y2=PC11/12**；Y3/Y4 **默认 PB1/2**（`EN_OV7670_LOCAL=1`）；无摄像头调试时改宏为 **0** 且列改 **PA5/6**。  
7. **SWD：PA13/PA14**；工程 **关 JTAG**，PB3/PB4 可作 GPIO（接 OV 时为 FIFO 脚）。接线分束见 **`硬件\硬件连接方案.md` §2**。

---

## 16. 排障矩阵（速查）

| 现象 | 优先查 |
|------|--------|
| OLED 无显示 | PB8/PB9 接线、`OLED_Probe()`、上拉电阻、供电 |
| gas 恒 0 | MQ2 是否接 PB0、`MQ2_Init()`、`Task_Sensor` 是否运行 |
| ESP 无 AT / OneNET 不上线 | PB10/PB11、GND、115200、供电、**EN(PC9)**、**AT 固件是否含 MQTT**、**`ONENET_*` 宏与控制台 Token** |
| `tls_inited` 一直 0 | UART1 搜 **`[OneNET]`** 日志；WiFi 2.4G；Broker 8883 |
| JSON 乱码 | USART1 波特率≠`uart1_Init` |
| 抓拍无文件 | 是否 **`EN_OV7670_LOCAL=1`**；SD/SPI2、OV 初始化；键盘 Y3/Y4 是否已接 **PB1/PB2**；`g_cap_q` |
| 指纹不灵 | PA9/10、**PA8 PS_Sta** |

---

## 17. 安全与电气（摘要）

燃气/继电器为**演示级**；真实燃气须合规施工。注意继电器驱动、电源裕量、**3.3V TTL**。接 **OV7670** 时 **PA12** 作 **FIFO RCK**；若同时使用 MCU **USB 从机** 功能，勿再占用 **PA12** 作 GPIO（与芯片 USB D+ 复用，见数据手册）。

---

## 18. 文档地图（建议更新）

| 文件 | 用途 |
|------|------|
| `使用说明书.md` | **人类操作主文档（纯单芯片版）**：编译下载、引脚/USART 分工、任务与抓拍流程、测试顺序、排障；已与当前 `main.c` / `board_config.h` 对齐 |
| `硬件/硬件连接方案.md` | **单芯片 RCT6** 全外设接线、**§2 杜邦线分束**、电源共地、`EN_OV7670_LOCAL` 与键盘 Y3/Y4 切换说明 |
| `硬件/工程真实硬件连接表.md` | **主机**引脚与源码对照 |
| `硬件/单芯片STM32F103RCT6智能安防系统_硬件调试Checklist.md` | **实机硬件分步调试清单**（上电前/OLED/键盘·JTAG/SD/ESP8266/OV7670） |
| `单芯片STM32F103RCT6智能安防工程重构规范.md` | **单芯片引脚分配与重构强约束** |
| `硬件/ESP8266说明.md` | ESP8266 / OneNET 接线与流程（需与源码互校） |
| `硬件/主机核心板排针接线表.md` | 排针丝印 ↔ MCU |
| `论文技术说明.md` | 论文撰写摘要 |
| （已删除）`Slave_F103/` | 从机工程已移除 |

---

## 19. 给 AI 的作答策略（单芯片版）

1. **先分类**：硬件接线 / 主从协议 / 单机主机功能 / WiFi·OneNET / 编译 Keil。  
2. **引脚问题**：先查 **`User\board_config.h`** 与 **`硬件\硬件连接方案.md`**（§4 与本节须一致），禁止编造 USART/键盘/SD 片选/PS_Sta/ESP EN 等用途。  
3. **协议问题**：直接回答“**已删除双机协议**”，不要编造帧结构。  
4. **抓拍**：默认 **`EN_OV7670_LOCAL=1`**，本机 **OV7670+FIFO** 写 SD(BMP)；`=0` 时占位不写盘。键盘 Y3/Y4 默认 **PB1/PB2**。  
5. **JSON vs OneNET**：调试 **JSON** 在 **USART1**；**OneNET 物模型** 经 **USART3→ESP8266**（缓冲符号 **`Uart2_Buf`**），**AT+MQTT**，**`Task_Wifi_MQTT`** + **`tls_inited`**。  
6. **裸 MQTT**：当前工程**不应**再描述 **TCP `CIPSTART` + 二进制 MQTT** 为主路径；以 **§12** 为准。  
7. **不确定**：写明「以当前 `main.c` / `esp8266_tls.c` 为准」并建议用户 grep **`ESP8266_OneNET_Full_Init`** / **`Task_Wifi_MQTT`**。

---

*本文件已按当前 `User\main.c`、`User\Capture.c`、`User\esp8266_tls.c`、`User\esp8266_onenet_mqtt.c/.h`、`User\board_config.h`、`硬件\硬件连接方案.md` 对齐；**ESP8266：USART3 PB10/PB11 + EN=PC9**（**非** PA2/PA3/PA4）；**`EN_OV7670_LOCAL` 默认 1**。若与源码冲突，**以源码为准**。*
