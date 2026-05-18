# 集成验证报告

**验证 Agent**：Agent-G  
**类型**：跨模块端到端集成验证  
**基于**：6 份模块验证报告 (module_A ~ module_F)

---

## 1. 端到端调用链验证

### 路径1: PIR入侵检测全链路

```
PIR EXTI4 ISR → IPC_NotifyPIR_FromISR(信号量)
  → Task_Sensor (PRIO=3, 100ms): HC_SR501_Poll_Triggered → HC_SR501_IsValidTrigger
  → MQ2_Read_ADC_Filter + DHT11_Read_Data
  → Fusion_IsHighConfidence (50*PIR + 30*Cam + 20*Gas >= 5000)
  → 告警队列 → Task_Log: Linkage_OnIntrusion → BEEP_StartPattern
  → MQTT: OneNET_Publish_Alarm → OLED: OLED_View_ShowAlarm
  → Capture_Request → Bare_CapturePoll
  → SysLog_Add
```

**状态**: ⚠️ 可部分走通，存在严重时序问题
- PIR消抖在RTOS 100ms周期下需10次确认=1秒（裸机160ms），设计偏差10x
- MQ2卡死检测仅在裸机路径实现，RTOS路径缺失
- DHT11指数退避仅在裸机路径实现
- 蜂鸣器BEEP_Tick10ms在RTOS下以100ms而非10ms调用，时序10x偏差
- BEEP/Linkage操作OLED无互斥保护

### 路径2: 密码开锁全链路

```
KeyboardSM_Tick (Task_Lock, 20ms) → Key_Scan (4×4矩阵)
  → LockManager_OnConfirm (#键)
  → FlashStore_ReadPasswords → SecStore_ReadCfg
  → AES128_CBC_Decrypt + HMAC验证
  → 密码匹配 → RELAY=0 (开锁) + RELAY_TIME=3
  → Linkage_OnUnlock (三声蜂鸣+照明)
  → MQTT: 门状态上报
  → SysLog_Add (UNLOCK事件)
  → TIM2 1秒递减 → RELAY=1 (关锁)
```

**状态**: ⚠️ 可基本走通，但存在阻塞点
- 管理员改密流程逻辑断链（P0-2, Agent-A）：pwd_chg_state标志不被检查
- Task_Log直接操作RELAY绕过FSM，与Task_Lock存在竞态（P1-1, Agent-A）
- Flash存储区与syslog重叠（P0跨模块）：密码写入可能破坏日志，反之亦然
- AES解密在Task_Lock 512字节栈上消耗~400字节，余量偏紧
- RELAY_TIME首次递减时间不精确（2~3s范围）

### 路径3: 远程命令全链路

```
MQTT订阅: AT+MQTTSUB → +MQTTSUBRECV 异步通知
  → OneNET_Parse_Cmd (Uart2_Buf[800]中搜索JSON)
  → 命令去重 (16槽环形缓冲, 60s过期)
  → 安全检查: LockManager_IsPwdLocked / LockManager_IsBruteAlarm
  → 命令分发: Ctrl_Led / Ctrl_Door (远程开锁) / Ctrl_Arm (布防)
  → capture/snapshot → Capture_Request(CAP_EVT_REMOTE)
  → threshold/pir_debounce → 修改g_pir_debounce_ms
  → MQTT reply: OneNET_AT_Mqtt_PublishRaw → +MQTTPUB
```

**状态**: ❌ 不可靠运行
- Uart2_Buf[800] ISR/Task无锁共享（P0-1, Agent-C）：JSON解析时ISR可能覆盖数据
- Task_Net栈1024字节余量仅~160字节（P0-2, Agent-C）
- ESP8266_CooperativeYield在RTOS下缺taskYIELD（P1-5, Agent-C）：长AT等待冻结同/低优先级任务
- MQTT TLS scheme=1 但 port=1883 不匹配（P1-3, Agent-C）
- 远程开锁安全检查正确实现
- 命令去重逻辑基本正确（ts语义混用但不影响功能）

---

## 2. 资源共享冲突

### USART1 (PA9/PA10) — 三重冲突

| 使用者 | 方向 | 场景 |
|--------|------|------|
| AS608指纹模块 | RX/TX | 指纹图像采集/搜索/注册，800ms超时 |
| printf调试输出 | TX only | LOG_NET/LOG_SYS/DeviceStatus_PrintMqttJson |
| SysLog_ReportLast | TX only | UART日志导出 |

**冲突后果**：printf/SysLog向PA9发送数据时，数据同时进入AS608的RX引脚，被指纹模块误解释为命令。AS608活跃时printf阻塞USART发送。**无任何互斥或仲裁机制**。

### SPI2 (PB12~PB15) — 当前独占

仅SD卡使用SPI2。无冲突。但 SPI_WriteByte 的 TXE/RXNE 等待无超时保护。

### Flash 末页 — secure_store vs syslog 重叠

```
SYSLOG_FLASH_BASE = 0x0801E000 (4页×2KB = 8KB, 覆盖 0x0801E000~0x08020000)
SECSTORE_ADDR     = 0x0801FF00 (在 syslog 末页内, 槽位248)
```

根本原因：`stmflash.h:10` 定义 `STM32_FLASH_SIZE=128`，但实际芯片为256KB。两个Agent（A和E）独立检出此问题，确认为**最严重的跨模块冲突**。

### OLED GRAM[128][8] — 无互斥并发访问

| 写入者 | 优先级 | 场景 |
|--------|--------|------|
| Task_OLED | PRIO=2 | 每200ms刷新仪表盘 |
| Task_Lock→SysLog_OLED_ShowPage | PRIO=5 | 用户按C键翻页日志 |

无任何互斥锁保护。Task_Lock优先级更高，可在Task_OLED刷屏中途抢占写入。

---

## 3. 中断优先级分析

| 中断 | 用途 | 优先级 |
|------|------|--------|
| SysTick | RTOS tick + g_bare_tick_ms | 最低(15) |
| EXTI4 | PIR人体检测 | 需查NVIC配置 |
| USART1 | AS608指纹 | 需查NVIC配置 |
| USART3 | ESP8266 AT/MQTT | 需查NVIC配置 |
| TIM2 | 继电器保持计时 | 需查NVIC配置 |

**分析**：
- FreeRTOS管理下中断优先级必须在 `configLIBRARY_LOWEST_INTERRUPT_PRIORITY` ~ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 之间
- DHT11读取时关全局中断4ms，阻塞所有ISR包括SysTick（RTOS心跳暂停）
- 未发现明确的中断优先级反转风险，但未找到NVIC_PriorityGroupConfig和具体优先级的文档化配置
- SWD(PA13/PA14)未被用作GPIO，保留调试功能

---

## 4. 启动初始化顺序

```
1. NVIC_PriorityGroupConfig ✓
2. delay_init ✓ (建立tick基准)
3~7. GPIO初始化 (ESP8266_EN/BEEP/RELAY/KEY) ✓
8~14. 外设初始化 (TIM2/PIR/DHT11/MQ2/OV7670/Capture) ✓
15. OLED_View_Init ✓
16~17. 门禁系统初始化 (LockManager/KeyboardSM) ✓
18. Linkage_Init ✓
19. SysLog_Init ✓
20. WDG_Init + WDG_HwInit ✓ (最后启动)
```

**评估**：初始化顺序无跨模块依赖冲突。`delay_init` 最早（提供tick），`WDG_Init` 最晚（避免长初始化触发WDT）。一个例外：`ESP8266_EN_GPIO_Init` 在第3步使能ESP8266，但USART3在第5步才初始化，中间无冲突。

---

## 5. 最终结论 — 代码是否可在STM32F103RCT6上运行

### 编译可行性 ✅
- 语法/类型无明显错误
- 头文件依赖链完整
- extern声明一致性良好（除Ctrl_Door/Ctrl_Arm悬空）
- 代码结构符合Keil MDK编译要求

### 运行可行性 ⚠️
**6个P0致命问题**阻止可靠运行：
1. **Flash地址重叠** (Agent-A/E交叉验证)：单行修复 `STM32_FLASH_SIZE=128→256`
2. **Syslog槽位不匹配**：32 vs 40字节，需同步修复
3. **Uart2_Buf ISR/Task数据竞争**：MQTT命令解析不可靠
4. **JTAG未释放** (PB3/PB4)：OV7670抓拍永久失效
5. **Task_Net栈溢出风险**：余量仅~160字节
6. **OV7670 VSYNC死循环**：摄像头故障时系统挂起

### 资源适配 ✅
- Flash使用：~44-56KB / 256KB (约20%)
- RAM使用：~10KB / 48KB (约20%)
- 资源充裕，有余量增加修复代码

### 核心场景评估

| 场景 | 可行性 | 阻塞问题 |
|------|--------|---------|
| 门禁密码开锁 | 基本可走通 | Flash重叠+改密断链+竞态条件 |
| 指纹开锁 | 基本可走通 | Task_Log直接操作RELAY绕过FSM |
| PIR入侵检测 | 严重劣化 | 消抖10x延迟+蜂鸣10x偏差 |
| MQTT远程控制 | 不可靠 | Uart2_Buf数据竞争+栈溢出风险 |
| 本地抓拍 | 永久失效 | JTAG未释放导致OV7670不工作 |
| OLED显示 | 基本可工作 | Toast不消失+GRAM无互斥 |
| 系统日志 | 全盘崩溃 | 槽位大小不匹配+Flash重叠 |

### 推荐修复优先级

**第一优先（单行修复即可解锁多个阻塞点）**：
- `stmflash.h`: `STM32_FLASH_SIZE 128 → 256`
- `syslog.h`: `SYSLOG_SLOT_SIZE 32 → 40` (或等效)
- `sys.c`或`main.c`: 添加 `GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)`

**第二优先（影响可靠性的设计修复）**：
- Uart2_Buf双缓冲或关中断保护
- ESP8266_CooperativeYield增加taskYIELD
- Task_Net栈增大至1536+

**结论**：代码架构合理，模块化清晰，资源利用充分。当前存在6个P0级问题，主要根因集中在3处（Flash配置错误、JTAG未释放、数据竞争）。修复这3处后，门禁和传感器核心功能预计可正常运行。MQTT通信和抓拍功能需要额外的栈和并发保护修复。
