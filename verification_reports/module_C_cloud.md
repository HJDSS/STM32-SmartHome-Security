# 模块组：云平台通信 — 验证报告

**验证 Agent**：Agent-C  
**验证范围**：esp8266_tls.c/h, esp8266_onenet_mqtt.c/h

---

## P0 致命问题

### P0-1: Uart2_Buf 无并发保护 — ISR 与任务级数据竞争
- 文件: `esp8266_tls.c:834,847,890-898`；`esp8266_onenet_mqtt.c:12,42,55`
- `Uart2_Buf[800]` 由 `USART3_IRQHandler`（ISR 写入）与所有任务级函数（`OneNET_Parse_Cmd`、`WaitSubstr`、`WaitOkOrError`、`WaitAny2` 等）无锁共享。`OneNET_Parse_Cmd` 执行期间（含多个 `strstr` + `memcpy` + `sprintf`）ISR 可在任意时刻追加写入新数据，导致 `strstr` 读到半写数据、`s_payload_snap` 拷贝截断、去重环形缓冲区读取到污染的命令 ID。

### P0-2: Task_Net 栈溢出风险 — OneNET_Publish_Data 局部数组 504 字节
- 文件: `esp8266_tls.c:755-756`；`board_config.h:548`
- `OneNET_Publish_Data` 栈帧声明 `char topic[120]` + `char jb[384]` = 504 字节。加上 `OneNET_AT_Mqtt_PublishRaw`（~170 字节）和 FreeRTOS 上下文（~100 字节），合计约 864 字节。Task_Net 栈仅 1024 字节，余量仅 ~160 字节。任何额外中断嵌套都可能触发 **HardFault 复位**。

---

## P1 严重问题

### P1-1: MQTT FSM state 3 混合阻塞调用，破坏 FSM 非阻塞契约
- 文件: `esp8266_onenet_mqtt.c:293-317`
- State 3 使用 `ESP8266_AT_SendWait` 和 `MqttAT_SendLongBlob`（全部阻塞同步函数），与 state 0/1/2/5/6/7/8 使用的非阻塞机制不一致。`MqttAT_SendLongBlob` 被调用 3 次（LONGCLIENTID + LONGUSERNAME + LONGPASSWORD），每次最长堵塞 8000ms，合计最长 **24 秒自旋**。

### P1-2: ESP8266_WiFi_Join 中 sprintf 无长度约束
- 文件: `esp8266_tls.c:327,337`
- `sprintf(cmd,"AT+CWJAP=\"%s\",\"%s\"",WIFI_SSID,WIFI_PASS)` 使用栈数组 `char cmd[128]`。若 SSID/PASS 超过 ~55 字节直接栈溢出。当前值安全，但属脆性设计。

### P1-3: MQTT TLS 模式与端口不匹配
- 文件: `esp8266_onenet_mqtt.h:21-28`
- `ONENET_MQTT_AT_SCHEME` 定义为 `1`（MQTT over TLS），但 `ONENET_MQTT_PORT` 定义为 `1883`（明文端口）。TLS MQTT 通常使用 8883。需确认 OneNET 平台实际要求。

### P1-4: ESP8266_OneNET_Full_Init 无限期阻塞
- 文件: `esp8266_tls.c:566-572`
- `while(!tls_inited)` 无超时保护。若 ESP8266 硬件损坏或 EN 引脚异常，系统在此死循环。

### P1-5: ESP8266_CooperativeYield 在 RTOS 模式下缺失 taskYIELD
- 文件: `main.c:141-144`
- `USE_FREERTOS=1` 时仅泵键盘状态机，未调用 `taskYIELD()`。所有 AT 等待循环调用此函数后继续自旋，同/低优先级任务得不到调度。影响：Task_Sensor 停采、Task_OLED 冻结、Task_Log 丢日志。

---

## P2 建议改进

1. **Ctrl_Arm 发布冗余** (`esp8266_tls.c:76,768-776`)：`Ctrl_Arm` 定义但已在 M1.16h 中移除上行
2. **OneNET_Publish_Data 的 id_n 线程不安全** (`esp8266_tls.c:757`)：static 非原子变量
3. **RELAY_TIME 类型为 u8 限制最大 255 秒** (`esp8266_tls.c:23`)
4. **OneNET_Parse_Cmd 去重环的 ts 语义可疑** (`esp8266_tls.c:856-876`)：本地时间与云时间戳混用
5. **LOG_NET 通过 printf 走 USART1 可能与 AS608 冲突** (`esp8266_tls.c:141-145`)
6. **阻塞型 WaitSubstr 对 Uart2_Buf[0]==0 的微妙时序** (`esp8266_tls.c:845`)
7. **MQTT FSM 的 msw 超时状态码与 WaitAny2_Nb_Poll 不统一** (`esp8266_onenet_mqtt.c:210-228`)

---

## 依赖与接口

| 接口符号 | 定义位置 | 引用位置 | 一致性 |
|---|---|---|---|
| `Uart2_Buf[800]` | main.c:48 | esp8266_tls.c (12处), esp8266_onenet_mqtt.c (6处) | 良好 |
| `CLR_Buf2()` | usart2.h:11 | esp8266_tls.c 多处 | 良好 |
| `UART2_SendString()` | usart2.h / usart2.c | 两文件多处 | OK (驱动 USART3) |
| `ESP8266_CooperativeYield()` | main.c:141 | esp8266_tls.h:27 声明，多处调用 | RTOS 下缺 taskYIELD |
| `RELAY` / `RELAY_TIME` | gpio.h / main.c | esp8266_tls.c:124-125 | 良好 |
| `Light_Relay_On/Off()` | linkage.h:46-47 | esp8266_tls.c:134-136 | 良好 |
| `Linkage_OnUnlock()` | linkage.h:38 | esp8266_tls.c:126 | 良好 |
| `LockManager_IsPwdLocked()` | lock_manager.h:13 | esp8266_tls.c:118 | 良好 |
| `LockManager_IsBruteAlarm()` | lock_manager.h:11 | esp8266_tls.c:118 | 良好 |
| `Capture_Request()` | capture_task.h:17 | esp8266_tls.c:1022 | 良好 |
| `IPC_NotifyCaptureReq()` | app_rtos.h:19 | esp8266_tls.c:1025 | 良好 |

---

## 资源估算

| 指标 | 值 |
|---|---|
| 全局 + static RAM | ~1720 B (Uart2_Buf 800B + payload_snap 512B + topic 128B + 其他) |
| Task_Net 栈 (board_config.h:548) | 1024 B |
| 栈峰值估算 (Publish 路径) | ~864 B，余量仅 ~160 B (**P0 风险**) |
| .text 估算 | ~8-12 KB |
| 最大单次 AT 超时 | 45000 ms (MQTT CONN) |
| 重连退避范围 | 1.2s → 60s (指数加倍) |
| JSON 解析方式 | strstr/sscanf 手工扫描 (无 JSON 库) |
| 并发保护 | **无** (Uart2_Buf ISR/Task 无锁共享) |

---

**总结**：云平台通信模块功能相对完整，FSM 设计合理（指数退避、多阶段重连）。但存在两个 P0 致命问题：Uart2_Buf 无锁数据竞争和 Task_Net 栈空间不足。P1-5（ESP8266_CooperativeYield 缺 taskYIELD）影响最广，会导致长 AT 等待期间所有同/低优先级任务冻结，对安防系统实时性影响严重。
