# 模块组：门禁系统 — 验证报告

**验证 Agent**：Agent-A  
**验证范围**：lock_manager.c/h, keyboard_sm.c/h, flash_store.c/h, secure_store.c/h, aes128.c/h, as608.c/h

---

## P0 — 致命问题（无法编译/立即崩溃/Flash 冲突）

### P0-1: Flash 容量配置错误导致 secure_store 与 syslog 存储区冲突

- 文件: `Driver/stmflash.h:10`
- 问题: `#define STM32_FLASH_SIZE 128`。目标芯片是 **STM32F103RCT6，实际 Flash 为 256KB**（STM32 命名规则中 'C' = 256KB）。但 `stmflash.h` 中配置为 128KB，导致 `secure_store.c` 计算出的 `SECSTORE_ADDR` 位置错误。
- 影响链:
  - `SECSTORE_ADDR` = `0x08000000 + 1024*128 - 256` = `0x0801FF00`
  - `SYSLOG_FLASH_BASE` = `0x0801E000`（syslog.h:6），占用 4 页 x 2048 = 8192 字节，覆盖 `0x0801E000~0x0801FFFF`
  - **两者在 `0x0801FF00~0x0801FFFF` 重叠 255 字节，且处于同一个 1KB 扇区（扇区127）**
  - 当 `SecStore_WriteCfg` 写入密码配置时，`stmflash.c` 会擦除整个扇区 127（`0x0801FC00~0x0801FFFF`），导致 syslog 最后 256 字节被清空
  - 反之 syslog 写满后回绕擦除扇区 127 时，加密的密码配置也会丢失

### P0-2: 管理员改密流程完全不可用（逻辑断链）

- 文件: `User/lock_manager.c:203-208`（函数键 D 处理）与 `LockManager_OnConfirm:87-152`
- 问题: 管理员按 D 进入改密状态后（`pwd_chg_state = 1u`），用户输入新 6 位密码后按 `#`。`KeyboardSM_Tick()` 调用 `LockManager_OnConfirm()`，但该函数**完全不检查 `pwd_chg_state` 标志**，总是将输入与现有密码比对。新密码永远不会被写入 Flash。
- 缺失的逻辑: 在 `LockManager_OnConfirm` 开头增加判断：若 `s_lock.pwd_chg_state != 0u`，则将 `in->digits` 通过 `FlashStore_WriteUserPassword()` / `FlashStore_WriteAdminPassword()` 保存，并复位 `pwd_chg_state`。

---

## P1 — 严重问题（逻辑错误/协议违规/安全漏洞）

### P1-1: Task_Log 直接操作 RELAY/RELAY_TIME 绕过门禁 FSM，存在竞态条件

- 文件: `User/app_rtos.c:426-427`
- 问题: `Task_Log`（优先级 3）在指纹匹配成功后，直接执行 `RELAY = 0; RELAY_TIME = APP_LOCK_OPEN_HOLD_TICKS;`，完全绕过 `lock_manager.c` 的 FSM。而 `Task_Lock`（优先级 5）中的 `LockManager_Tick()` 也同时操作 `RELAY` 和 `RELAY_TIME`。两个不同优先级的任务对同一 GPIO 和全局变量写入，无互斥保护。
- 后果: 若 Task_Lock 在 `LockManager_Tick:325-339` 中检测到 `RELAY_TIME == 0u` 正准备把 `RELAY = 1`（关锁），而 Task_Log 恰好在此时写入 `RELAY = 0`（开锁），可能导致门锁继电器快速切换或状态不一致。

### P1-2: LockManager_GetState() 返回内部状态指针，无互斥保护

- 文件: `User/lock_manager.c:82-85`, `User/app_rtos.c:388`
- 问题: `LockManager_GetState()` 直接返回 `&s_lock` 指针。`Task_OLED`（优先级 2）读取整个 `lock_state_t` 结构体。同时 `Task_Lock`（优先级 5）可在 `LockManager_Tick()` 中修改 `s_lock` 的多个字段。Cortex-M3 上 32 位访问是原子的，但结构体跨多个字，`Task_OLED` 可能读取到新旧混合的状态。

### P1-3: 固件中硬编码 AES 主密钥，且为公开常数

- 文件: `User/secure_store.c:58`
- 问题: `static const u8 master[16]` 是一个硬编码在固件中的常量，编译后直接存放在 Flash 的 `.rodata` 段。任何能物理读取芯片或获取固件二进制的人都可以提取此密钥。虽然有 UID 绑定，但主密钥是固定的，攻击者提取一个固件便可推导所有同型号设备的存储密钥。

### P1-4: AS608_Find_Fingerprint() 在 Task_Log 中阻塞长达 800ms

- 文件: `Driver/as608.c:277-291`, `User/app_rtos.c:421`
- 问题: `AS608_Find_Fingerprint()` 内部 `uart_recv()` 超时设为 `AS608_UART_RECV_TIMEOUT_MS = 800u`。若每次轮询都无手指触摸，`AS608_Cmd_Get_Img()` 会等满 800ms 才超时返回。Task_Log 的 200ms 周期实际变为 800ms+，显著影响抓拍轮询和看门狗喂狗的及时性。

### P1-5: AES-128 CBC Encrypt/Decrypt 的 len 参数为 u16，但缺少长度对齐检查

- 文件: `User/aes128.c:203,219`, `User/secure_store.c:163,170,212`
- 问题: `AES128_CBC_Encrypt` / `AES128_CBC_Decrypt` 的 `plain_len` / `cipher_len` 声明为 `u16`，注释要求必须是 16 的倍数。但函数内部无长度对齐检查——若传入非 16 倍数的值，`CBC_Decrypt` 的 `off += 16` 循环会多解密一个不完整的块，导致越界访问。

### P1-6: RELAY_TIME 类型为 u8，最大只能支持 255 秒

- 文件: `User/main.c:32` (`volatile u8 RELAY_TIME`), `User/app_params.h:72`
- 问题: `APP_LOCK_OPEN_HOLD_TICKS` 将毫秒转为 tick，当前开锁保持 3000ms → 3 ticks，远小于 255。但若未来将 `APP_LOCK_OPEN_HOLD_MS` 调大（如 5 分钟=300000ms→300 ticks），就会溢出 u8。

---

## P2 — 建议改进（代码质量/可维护性）

1. **FSM 状态枚举 DOOR_ACQUIRE 和 DOOR_MATCH 定义但从未产生** (`app_types.h:31-32`, `lock_manager.c:52-62`)：枚举定义了但实际运行时 `DOOR_MATCH` 完全未使用（比对是瞬间完成的）
2. **FlashStore_ReadPasswords 每次调用都重新读 Flash 并解密** (`flash_store.c:14-30`)：每次按 `#` 都执行完整的 Flash 读取 + AES-CBC 解密 + HMAC 验证，耗时 >10ms
3. **as608.c 全局缓冲区 AS608_RECEICE_BUFFER 无并发保护** (`as608.c:27`)：所有 AS608 命令函数共用此缓冲区
4. **键盘输入超时宏 KEYBOARD_INPUT_TIMEOUT_MS 建议可配置化** (`keyboard_sm.c:13-14`)：未在 board_config.h 或 app_params.h 中声明默认覆盖宏
5. **LockManager_OnFunctionKey 中日志翻页状态违反单一职责** (`lock_manager.c:164-165`)：日志翻页的 static 局部变量放在 lock_manager.c 中不合适
6. **sprintf/strcpy/memcpy 安全性检查通过**：在所有审查的文件中，未发现不安全使用
7. **AES128_CTX 结构体 rk[176] 在栈上分配** (`aes128.h:6-9`)：Task_Lock 栈 512 字节，最深层调用链（AES 解密）可能消耗约 380~420 字节，余量偏紧

---

## 依赖检查

| 调用者 | 被调用的外部函数/变量 |
|--------|----------------------|
| `lock_manager.c` | `FlashStore_ReadPasswords`, `Bare_GetTickMs`, `RELAY`, `RELAY_TIME`, `OLED_ShowString`, `Linkage_OnUnlock`, `SysLog_Add`, `BEEP_StartPattern`, `Capture_Request`, `Security_Set_ArmMode`, `AS608_IsLockedOut` |
| `keyboard_sm.c` | `Key_Scan`, `Bare_GetTickMs`, `LockManager_OnConfirm`, `LockManager_OnFunctionKey`, `OLED_View_OnPasswordDigit` |
| `flash_store.c` | `SecStore_Init`, `SecStore_ReadCfg`, `SecStore_WriteCfg` |
| `secure_store.c` | `STMFLASH_Read`, `STMFLASH_Write`, AES 全家桶函数 |
| `as608.c` | `Usart1RecBuf`, `RxCounter`, USART1 驱动 |

## 资源估算

| 模块 | Flash 估算 | RAM 估算 |
|------|-----------|---------|
| lock_manager | ~3.5 KB | ~20 B (s_lock) |
| keyboard_sm | ~1.2 KB | ~8 B (s_input) |
| flash_store | ~0.6 KB | - |
| secure_store | ~4.5 KB | ~33 B (密钥窗口) |
| aes128 | ~5.5 KB | - |
| as608 | ~3.0 KB | ~29 B (缓冲区+计数) |
| **合计** | **~18.3 KB** | **~110 B** |

**Task_Lock 栈风险评估**: 512 字节，最深调用链（AES 解密）可能消耗 ~400 字节，余量偏紧。建议启用 FreeRTOS 栈溢出检测并在调试阶段监控 `uxTaskGetStackHighWaterMark(NULL)`。
