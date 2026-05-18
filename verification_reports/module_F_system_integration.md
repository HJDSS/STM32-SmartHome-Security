# 模块组：系统集成层 — 验证报告

**验证 Agent**：Agent-F  
**验证范围**：main.c, app_rtos.c/h, board_config.h, app_params.h, app_types.h

---

## P0 致命问题

### P0-1: GPIO_PinRemapConfig(SWJ_JTAGDisable) 缺失 — PB3/PB4 卡在 JTAG 模式
- 文件：`System/sys/sys.c`、`Driver/ov7670_fifo.c:13`
- STM32F103 复位后 PB3=JTDO、PB4=nJTRST。OV7670 FIFO 使用 PB3 (WRST) 和 PB4 (RRST) 作为 GPIO
- `GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)` **在整个代码库中没有被调用**
- AFIO 时钟使能了但从未写入 AFIO_MAPR 寄存器释放 PB3/PB4
- `board_config.h:15` 注释声明"关 JTAG、保留 SWD"，但代码未实施
- **后果**：OV7670 的 WRST 和 RRST GPIO 不工作，本地抓拍功能永久失效

### P0-2: EventGroup 的三个 bit 只发不收 — IPC 机制形同虚设
- 文件：`app_rtos.c:105,112,119`、`app_types.h:100-102`
- 定义了 EVT_ARM_STATE, EVT_NET_ONLINE, EVT_CAPTURE_REQ 三个事件位
- 全库搜索**没有任何地方调用** `xEventGroupWaitBits()` 等消费者函数
- EventGroup handle 创建后只有 setter 被调用，没有任何任务等待或读取
- 纯死代码，浪费约 28 字节 RAM

---

## P1 严重问题

### P1-1: BEEP_Tick10ms 实际调用周期与命名不符 — 蜂鸣音时序偏差 2~10 倍
- 文件：`main.c:658`（裸机 20ms）、`app_rtos.c:336`（RTOS 100ms）、`gpio.c:117-161`
- `BEEP_Tick10ms()` 设计为每 10ms 调用一次
- 裸机模式：每 20ms → 蜂鸣时长 2x
- RTOS 模式：每 100ms (Task_Sensor) → 蜂鸣时长 10x
- 实际体验：门禁声 3 秒、入侵声变慢速嘀嗒

### P1-2: SEC_RECOVERY 状态定义但不可达 — 论文五态 FSM 仅实现四态
- 文件：`app_types.h:39-46`、`main.c:543-551`、`app_rtos.c:326-333`
- `SEC_RECOVERY = 4` 在 enum 中定义，但告警解除后直接跳回 DISARMED，跳过恢复态
- 任何路径都无法到达 RECOVERY

### P1-3: TIM2 RELAY_TIME 首次递减与 time_count 相位未对齐
- 文件：`timer.c:35-70`、`app_params.h:71-73`
- `TIM2_IRQHandler` 内 `time_count` 是 static 变量，设置 RELAY_TIME=3 时 time_count 可能处于 0~99，首次递减延时不确定
- 实际保持时间范围 2~3s（而非精确 3s）

### P1-4: Task_Sensor 和 Task_Log 同优先级(3) — 时间片轮转而非严格分层
- 文件：`app_rtos.c:48-60`
- 设计文档描述 "net(4) > sensor(3) > log(3) > oled(2)"，但 sensor 和 log 同级会时间片轮转

### P1-5: 裸机 app_poll_net 中 ESP8266 发送阻塞主循环
- 文件：`main.c:592-596`
- `OneNET_Publish_Data` 同步阻塞，数据未发送完时下一次主循环到来，`CLR_Buf2()` 可能清除未处理响应

---

## P2 建议改进

1. EventGroup bit 位死代码建议清理
2. `APP_LOCK_OPEN_HOLD_TICKS` 预处理截断风险
3. `Bare_GetTickMs()` 读取 `g_bare_tick_ms` 未标注原子性
4. 多个 extern 声明在 .c 文件中重复散落，建议集中到 globals.h
5. Task_OLED 持有 `s_sensor_mtx` 时间过长（含 I2C 刷屏 ~60ms）
6. `Ctrl_Door`/`Ctrl_Arm` 在头文件有 `extern` 声明但全库无定义

---

## Init 调用顺序分析

```
1. NVIC_PriorityGroupConfig → 2. delay_init → 3. ESP8266_EN_GPIO → 4. uart1_Init (AS608)
→ 5. USART2_Init (ESP8266 UART) → 6. BEEP+RELAY GPIO → 7. Actuator_EnterSafeState
→ 8. Key_Init → 9. TIM2_Init → 10. HC_SR501_Init → 11. DHT11_Init → 12. MQ2_Init
→ 13. OV7670_FIFO_Init → 14. Capture_Task_Create → 15. OLED_View_Init
→ 16. LockManager_Init → 17. KeyboardSM_Init → 18. Linkage_Init → 19. SysLog_Init
→ 20. WDG_Init + WDG_HwInit
```

**顺序评估**：`delay_init` 最先（建立 tick 基准），`WDG_Init` 最后（避免长时初始化触发 WDT 复位）。依赖关系正确。

## extern 一致性

| 变量 | 状态 |
|------|------|
| g_bare_tick_ms, RELAY_TIME, security_mode, security_alarm, arm_mode | ✅ 一致 |
| dht11_temp, dht11_humi, mq2_adc_value | ✅ 一致 |
| g_cap_* 系列 | ✅ 一致 |
| **Ctrl_Door** | ❌ extern 声明但无定义 |
| **Ctrl_Arm** | ❌ extern 声明但无定义 |

---

## 资源估算

| 项目 | 估算 |
|------|------|
| 芯片 | STM32F103RCT6 (256KB Flash, 48KB RAM) |
| FreeRTOS 内核 | ~8-10 KB Flash |
| StdPeriph 库 | ~6-8 KB Flash |
| FatFs | ~4-5 KB Flash |
| 应用层代码 | ~20-25 KB Flash |
| 驱动层 | ~6-8 KB Flash |
| **预计总 Flash** | **~44-56 KB** (256KB 占比 ~20%) |
| 全局/静态 RAM | ~4-6 KB |
| FreeRTOS 任务栈 (5 tasks) | ~3 KB |
| FreeRTOS TCB + 内核对象 | ~700 B |
| 捕获 BMP 帧缓冲 | ~1 KB |
| FreeRTOS Heap 配置 | 24 KB (实际使用 ~4 KB) |
| **预计总 RAM** | **~10 KB** (48KB 占比 ~20%) |
| **栈溢出风险** | 低 (configCHECK_FOR_STACK_OVERFLOW=1 + Hook 已实现) |
| **堆耗尽风险** | 极低 (20KB 剩余) |

**结论**：Flash 和 RAM 均有 75%+ 余量，资源充裕。
