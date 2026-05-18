# 模块组：传感器系统 — 验证报告

**验证 Agent**：Agent-B  
**验证范围**：dht11.c/h, mq2.c/h, hc_sr501.c/h, linkage.c/h

---

## P0 — 致命问题（无法编译/立即崩溃）

（无）

经全文审查，所有 .c/.h 文件语法正确，类型系统一致，宏定义完整，无缺失分号/括号等编译阻断问题。

---

## P1 — 严重问题（逻辑错误/公式实现错误/数据处理缺陷/协议违规）

### P1-1: MQ-2 方差计算存在 u32 溢出风险

- 文件：`Driver/mq2.c`，第 153-154 行
- 问题：`u32 diff = ... (Q8 格式)`，最大可能值约 `4095 * 256 ≈ 1,048,320`。随后 `u32 diff_sq = (diff * diff) >> 8`，`diff * diff` 可达 `1,048,320^2 ≈ 1.1e12`，远超 `u32` 上限 `4.29e9`，造成静默回绕。

### P1-2: FreeRTOS 路径缺少 MQ-2 传感器卡死检测

- 文件：`User/app_rtos.c`，`Task_Sensor` 函数
- 问题：论文要求的"连续 40 次 ADC 变化不超过 3 则判定传感器故障"逻辑仅在 `main.c` 裸机路径实现。FreeRTOS 路径 `Task_Sensor` 中完全没有调用此检测逻辑。MQ-2 传感器故障时，FreeRTOS 模式下会持续使用卡死值更新基线/判定报警，导致误报或漏报。

### P1-3: FreeRTOS 路径缺少 DHT11 指数退避重试机制

- 文件：`User/app_rtos.c`，第 147-167 行 vs `User/main.c`，第 222-268 行
- 问题：裸机路径实现了完整的指数退避（连续失败 3 次后间隔从 2000ms 翻倍至最大 16000ms）。FreeRTOS 路径 `Task_Sensor` 中 DHT11 始终以固定 `APP_DHT_POLL_MS`（2000ms）轮询，不根据失败次数调整间隔。

### P1-4: PIR 软件消抖时间在 FreeRTOS 路径下严重失真

- 文件：`Driver/hc_sr501.c`，`HC_SR501_Poll_Triggered` 函数
- 问题：消抖算法用 `(g_pir_debounce_ms + 9u) / 10u` 将毫秒转换为"10ms 滴答数"。FreeRTOS 路径 `Task_Sensor` 周期为 100ms，8 次确认 = 800ms（10x 偏差）。RTOS 模式下 PIR 触发后需要约 800ms 连续高电平才能确认，严重延迟入侵检测响应。

### P1-5: DHT11 读取时全局关中断时间过长（~4ms）

- 文件：`Driver/dht11.c`，`DHT11_EnterCritical`/`DHT11_ExitCritical`
- 问题：`DHT11_Read_Data` 读取 5 个字节（40 bit）时调用 `__disable_irq()` 关闭全部中断约 4ms。这期间 SysTick 无法递增，FreeRTOS 调度时钟被暂停，所有外部中断（包括 EXTI4/PIR）被延迟。

### P1-6: Linkage_OnGasState 中直接操作 OLED 无互斥保护

- 文件：`User/linkage.c`，第 122、130 行
- 问题：`Linkage_OnGasState` 直接调用 `OLED_ShowString` 写入告警文字，但未获取任何 OLED 互斥锁。在 FreeRTOS 下，该函数从 `Task_Sensor`（优先级 3）调用，而 `Task_OLED`（优先级 2）也操作 OLED。OLED I2C 软件驱动未设计为可重入，两个任务并发写入会导致 OLED 显示花屏或内容撕裂。

### P1-7: MQTT 远程命令处理的门控条件语义错误

- 文件：`User/linkage.c`，第 168-180 行
- 问题：MQTT 命令处理函数实际生效依赖 `LINKAGE_TIMER_EN` 宏，但该宏的原意是"定时器相关联动使能"，与 MQTT 命令处理无逻辑关联。如果关闭 `LINKAGE_TIMER_EN`，所有远程命令将失效。

### P1-8: MQ2_BASELINE_ALPHA_Q8 精度偏差（轻微）

- 文件：`Driver/mq2.h`，第 18 行
- 问题：`MQ2_BASELINE_ALPHA_Q8 = 230`，对应 `230/256 = 0.8984375`，与论文 0.9 的偏差为 0.17%。工程可接受（<2% 误差）。

---

## P2 — 建议改进（代码质量/可维护性/性能优化）

1. **MQ-2 中值滤波使用冒泡排序（O(n^2)）** (`mq2.c:70-78`)：5 元素中值可通过 6 次比较确定，当前冒泡排序非最优
2. **DHT11_Read_Bit 函数未被调用** (`dht11.c:77-95`)：死代码，可移除以减少 Flash 占用
3. **DHT11 校验和的类型转换不必要** (`dht11.c:179`)：显式 `(u8)` 转换不改变语义但降低可读性
4. **DHT11_Debug_ReadFrame 硬编码魔术数字** (`dht11.c:224-236`)：建议使用 `#define` 防御越界
5. **MQ2_Get_Value 校准表硬编码且不可修改** (`mq2.c:197-198`)：无法在运行时重新标定
6. **PIR 三段式抑制的时间基准依赖 FreeRTOS tick 周期** (`app_rtos.c:198`)：若 `configTICK_RATE_HZ!=1000` 时间计算错误
7. **Linkage_OnUnlock 中开锁来源区分已丢失** (`linkage.c:67-84`)：`UNLOCK_SRC_T src` 参数被 `(void)src` 丢弃
8. **蜂鸣器模式与 Linkage 的交互缺少状态机保护**：同时有入侵和燃气告警时，后调用的蜂鸣模式覆盖前者
9. **ADC 滤波结果的饱和保护**：从不合理的高 ADC 值到滤波输出无显式保护（影响小，ADC 硬件上限 4095）
10. **Task_Sensor 栈大小（256 字=1024 字节）偏紧**：调用链深，累积估算 ~524 字节，建议部署前用 `uxTaskGetStackHighWaterMark()` 实测

---

## 依赖检查

| 本组模块 | 调用的外部函数/变量 | 所在文件 |
|---------|-------------------|---------|
| dht11.c | `delay_us`, `delay_ms`, `GPIO_*`, `__disable_irq`/`__enable_irq` | System/delay, STM32 StdPeriph, CMSIS |
| mq2.c | `ADC_*`, `GPIO_*`, `RCC_*` | STM32 StdPeriph |
| hc_sr501.c | `GPIO_*`, `EXTI_*`, `NVIC_*`, `IPC_NotifyPIR_FromISR`, `portYIELD_FROM_ISR` | STM32 StdPeriph, app_rtos, FreeRTOS |
| linkage.c | `GPIO_*`, `uart1_SendStr`, `OLED_ShowString`, `SysLog_Add`, `OneNET_Publish_Alarm`, `BEEP_StartPattern`, `GAS_VALVE_RELAY` | STM32 StdPeriph, 各驱动/应用模块 |

## 资源估算

| 模块 | Flash 估算 | RAM 估算 |
|------|-----------|---------|
| dht11 | ~1.2 KB | ~12 B |
| mq2 | ~2.5 KB | ~31 B (滤波窗口+基线+方差) |
| hc_sr501 | ~1.5 KB | ~16 B |
| linkage | ~2.0 KB | ~10 B |
| **合计** | **~7.7 KB** | **~55 B (驱动) + ~54 B (应用) + ~1.3 KB (RTOS开销)** |

**Task_Sensor 栈峰值估算**: 累积约 524 字节（DHT11 读取 + MQ2 滤波 + 联动调用），1024 字节栈空间够用但余量偏紧。建议将 `APP_TASK_STACK_SENSOR` 调至 384（1536 字节）。

---

**总结**：传感器系统模块整体设计合理，论文公式基本正确实现到代码中。主要风险事项为：
1. PIR 消抖时间在 FreeRTOS 下严重失真（10x 偏差）
2. FreeRTOS 路径缺失 MQ-2 卡死检测和 DHT11 指数退避两项防御机制
3. MQ-2 方差计算存在理论上的 u32 溢出风险
4. Linkage 操作 OLED 无互斥保护可能导致显示异常
