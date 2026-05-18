# 模块组：显示+日志+看门狗 — 验证报告

**验证 Agent**：Agent-E  
**验证范围**：oled_view.c/h, syslog.c/h, wdg.c/h

---

## P0 致命问题

### P0-1: Syslog SYSLOG_SLOT_SIZE(32) 与 LOG_REC_T(40字节) 不匹配 — 每条记录写入破坏相邻槽位
- 文件：`syslog.h:9` + `syslog.c:14-26`
- `#define SYSLOG_SLOT_SIZE 32`，但 `LOG_REC_T` 实际大小 40 字节
- `slot_addr()` 用 32 字节步进，但 `SysLog_Add()` 写入 40 字节 → 每条记录溢出 8 字节到相邻槽位
- `SYSLOG_MAX_SLOTS = 4*2048/32 = 256` 错误，实际非重叠应为 `8192/40 = 204`
- **影响**：Flash 环形日志所有读写操作均越界，CRC 校验大量失败，环形缓冲区逻辑崩溃

### P0-2: secure_store 与 syslog 的 Flash 地址区间重叠
- 文件：`secure_store.c:11` + `syslog.h:6` + `stmflash.h:10`
- `SYSLOG_FLASH_BASE = 0x0801E000`，占用 8KB 至 `0x08020000`
- `stmflash.h` 定义 `STM32_FLASH_SIZE = 128`（实际 RCT6 为 256KB）
- `SECSTORE_ADDR = 0x0801FF00` 落在 syslog 末页（槽位 248）
- 两个模块互相破坏对方数据

---

## P1 严重问题

### P1-1: OLED_View_ToastRowAB 的 duration_ms 参数完全未使用，Toast 从不自动清除
- 文件：`oled_view.c:80-97`
- `(void)duration_ms;` — 参数被显式忽略
- Toast 仅能被后续 dashboard 刷新覆盖，调用者传入的 1200ms/2000ms 期望值全部无效

### P1-2: SysLog_OLED_ShowPage 从 Task_Lock(PRIO=5) 直接写 OLED，与 Task_OLED(PRIO=2) 无互斥
- 文件：`syslog.c:224-252` + `lock_manager.c:270`
- `SysLog_OLED_ShowPage` 直接操作 `OLED_GRAM[128][8]` 全局缓冲区
- 同时 Task_OLED 也操作同一 GRAM，无任何互斥保护 → 显示撕裂

### P1-3: SysLog_Add 的 Flash 擦写操作可能耗时过长
- 文件：`syslog.c:149-155`
- `FLASH_ErasePage` 单页擦除 20-40ms，期间可能关闭中断
- 高优先级任务调用时影响 SysTick/NVIC 和 WDG 喂狗

### P1-4: SysLog_ReportLast 使用 uart1_SendStr 与 AS608 指纹模块冲突
- 文件：`syslog.c:217-218`
- USART1 同时连接 AS608，发送调试数据会干扰指纹模块

### P1-5: WDG_Mark/WDG_Pump 在多任务下无原子保护
- 文件：`wdg.c:91-132`
- `s_src_last_kick_ms[]` 被多任务并发访问，读取-计算-比较序列非原子

---

## P2 建议改进

1. OLED 密码星号位置硬编码像素偏移
2. SysLog_Add 中 strncpy 不保证终止（被前置 memset 保护，但脆弱）
3. WDG_Init 中 3000ms 魔法值未用宏定义
4. sprintf 可改用 snprintf 防御性编程
5. OLED 任务在 `s_sensor_mtx` 锁内执行 I2C 刷屏 ~60ms，阻塞时间过长

---

## 依赖与接口

**OLED 依赖链**：
```
OLED_View_RefreshDashboard
  ├── sensor_state_t *sensor → g_sensor (app_rtos.c)
  ├── lock_state_t *lock → LockManager_GetState() → &s_lock (lock_manager.c)
  ├── esp_state_t *esp → g_esp (app_rtos.c)
  └── OLED_ShowString → Driver/oled.c (软件I2C PB8/PB9 → SH1106)
```

**Syslog 调用者**（21 处写入口，5 个模块）：
| 调用模块 | 事件类型 |
|---------|---------|
| main.c | BOOT, RST_IWDG, DHT_OFFLINE, NET_OFFLINE 等 |
| app_rtos.c | PIR_INTRUSION, FP_UNLOCK, STACK_OVF, MALLOC_FAIL 等 |
| lock_manager.c | BRUTE_FORCE, ARM_AWAY/DISARM, PWD_CHG 等 |
| Capture.c | SD_MOUNT_OK/FAIL, CAP_BEGIN/END_OK 等 |
| linkage.c | GAS_VALVE_CLOSE, LIGHT_ON/OFF 等 |

**WDG 标记源 vs 调用任务**：5 个源分别由 Task_Sensor/Task_Net/Task_OLED/Task_Log 标记，WDG_Pump 由 Task_Log 调用

---

## 资源估算

| 项目 | 值 |
|------|-----|
| Task_OLED 栈 | 1024 B (256 words)，峰值 ~400-500B，**充裕** |
| I2C 刷屏耗时 | ~60ms (200ms 周期内 CPU 占用 ~30%) |
| SysLog Flash 写入 (无擦除) | ~0.5ms |
| SysLog Flash 写入 (需擦除) | ~20.5ms (每 51 条触发一次) |
| IWDG 超时 | 10 秒 (LSI 40kHz / 256 分频) |
| WDG 各源阈值 | 80% × 10s = 8s (UI≥3s, SYSMON=9s) |
