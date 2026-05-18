# 模块组：摄像头+存储 — 验证报告

**验证 Agent**：Agent-D  
**验证范围**：Capture.c/h, capture_task.c/h, sd_spi.c/h, sd_capacity.c/h, ov7670_fifo.c

---

## P0 致命问题

### P0-1: `capture_one_to_sd()` 调用不带超时的 `OV7670_FIFO_StartCapture()`，可能永久阻塞系统
- 文件：`Capture.c:267`，底层 `ov7670_fifo.c:78-89`
- `capture_one_to_sd()` 调用 `OV7670_FIFO_StartCapture()`（不带超时），内部是两个死等 while 循环 `while(OV_VSYNC_READ()==0);` 和 `while(OV_VSYNC_READ()==1);`，无超时退出机制
- 当 OV7670 未正确初始化、FIFO 异常或 VSYNC 引脚故障时，CPU 在此永久阻塞
- FreeRTOS 下 Task_Log 永久挂起，WDG_Pump 不再执行，10 秒后 IWDG 复位系统
- 同文件中 `Capture_LocalSnapshot()` 使用带 800ms 超时的版本，说明开发者已知此风险

---

## P1 严重问题

### P1-1: `capture_one_to_sd()` 未设置 `g_capture_busy` 标志，与 `Capture_LocalSnapshot()` 存在竞态
- 文件：`Capture.c:229-303 vs 305-435`
- `Capture_LocalSnapshot()` 通过 `g_capture_busy` 实现临界区保护，但 `capture_one_to_sd()` 全程不操作该标志
- RTOS 下两函数可能同时操作 OV7670 FIFO + SD SPI，导致数据错乱

### P1-2: `Capture_OnSaved()` 对同一个抓拍事件双重 MQTT 发布
- 文件：`Capture.c:445-465`
- 同时调用 `Linkage_MQTT_Report` 和 `capture_publish_saved`，云端收到两条语义重复的消息

### P1-3: `capture_publish_saved()` 中 topic/payload 缓冲区溢出风险
- 文件：`Capture.c:83-94`
- 使用 `sprintf` 无长度保护，若更换为更长标识符可能溢出

### P1-4: `sd_capacity.c` 全模块为编译占位实现，SD 容量统计完全不可用
- 文件：`sd_capacity.c:32-51`
- `SdCapacity_FetchInfo()` 永远返回 mounted=0, total_gb=0
- OneNET 物模型 JSON 完全不含 SD 字段
- FatFs 包含路径配置不一致

### P1-5: `SD_SPI_Init()` 使用占位扇区数而非读取 CSD 寄存器
- 文件：`sd_spi.c:252`
- 固定使用 `SD_SPI_PLACEHOLDER_SECTORS = 8388608 (4GiB)` 而非 CMD9 读取真实容量

### P1-6: `Bare_CapturePoll` 在裸机(20ms)和RTOS(200ms)中调用周期不一致
- 文件：`Capture.c:467-484`，`main.c:663`，`app_rtos.c:443`
- RTOS 下离线队列 flush 延迟增大 10 倍，`g_cap_q_drop` 更容易递增

---

## P2 建议改进

1. `capture_one_to_sd()` 和 `Capture_LocalSnapshot()` 存在大量重复代码（~70%）
2. `Capture_Request()` 忽略 evt 参数，所有抓拍请求无差别
3. 静态行缓冲区 `s_cap_row_wr[640]` 仅被 `Capture_LocalSnapshot` 使用但占全局 RAM
4. `Bare_CapturePoll` 中 `delay_ms(50)` 在 RTOS 下阻塞任务
5. sd_spi SPI_WriteByte 的 TXE/RXNE 等待无超时保护
6. SPI 速度仅 Prescaler_32 (1.125MHz)，可提高到 Prescaler_4 (9MHz)
7. sd_capacity printf 输出到 USART1 与 AS608 冲突
8. `DeviceStatus_BuildOneNETThingPropertyJson` 使用静态递增 `prop_id` 未考虑溢出
9. `snprintf` 浮点数格式化增加 ~4-8KB 代码体积
10. 抓拍统计 `volatile u16` 65535 次后溢出
11. 文件名硬编码 `0:` 前缀依赖 FatFs 驱动映射

---

## 依赖与接口

**跨模块抓拍调用链**：
```
PIR/MQ2/远程命令触发 → Capture_Request(CAP_EVT_*)
  → g_cap_evt_pending = 1  /  IPC_NotifyCaptureReq (EventGroup)
  → Task_Log (200ms) / Bare_CapturePoll → capture_one_to_sd()
    → OV7670 FIFO 读取 → SD BMP 写入 → Capture_OnSaved()
      → Linkage_MQTT_Report / capture_publish_saved → MQTT 上报
      → capture_enqueue_saved → 离线队列 (MQTT 失败时)
```

| 导出接口 | 被谁调用 |
|---------|---------|
| `Capture_LocalSnapshot()` | esp8266_tls.c (远程), main.c (裸机) |
| `Bare_CapturePoll()` | main.c (裸机), app_rtos.c Task_Log |
| `Capture_Request()` | app_rtos.c, lock_manager.c, esp8266_tls.c |
| `SD_SPI_Init/Read/Write` | FatFs diskio 层 |
| `DeviceStatus_BuildMqttJson` | esp8266_onenet_mqtt.c (周期上报) |

---

## 资源估算

| 项目 | 大小 |
|------|------|
| FatFs 文件系统对象 | ~560 B |
| 离线抓拍队列 (8条目) | ~296 B |
| BMP 行缓冲 (全局) | 640 B |
| BMP 行缓冲 (栈, capture_one_to_sd) | 640 B |
| MQTT topic+payload 缓冲区 (栈) | 312 B |
| 其他临时缓冲区 | ~200 B |
| **RAM 合计** | **~2.7 KB** |
| Flash (Capture + sd_spi + sd_capacity + ov7670_fifo) | ~11-20 KB |
| **Task_Log 栈峰值** | ~800-1000 B (1024 栈余量偏紧) |

---

**总结**：P0 级 VSYNC 死等问题会导致系统在摄像头故障时完全挂起。SD 容量统计全模块为占位实现。离线队列设计合理但 RTOS 和裸机路径轮询频率不一致。
