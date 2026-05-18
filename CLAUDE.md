# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

STM32F103RCT6-based smart home security system — a graduation thesis project (广东工业大学). The firmware runs on a single-chip Cortex-M3 @72MHz, integrating door access (password + fingerprint), multi-sensor intrusion detection (PIR + MQ-2 + OV7670 camera), environmental monitoring (DHT11), cloud connectivity (ESP8266 → OneNET MQTT), OLED display, and SD storage.

**Thesis**: `论文相关/源文件/论文.tex` (LaTeX, compiled PDF at `论文相关/源文件/论文.pdf`). Code-thesis alignment was verified and 35+ gaps fixed across 10 commits (M1.15–M1.16h branch `rescue/auto-rollback-20260427-0000`). Detailed gap analysis at `thesis_code_gap_analysis.md` and fix report at `FIX_REPORT.md`.

## Build system

- **IDE**: Keil MDK (µVision 5). Project file: `biye/代码/User/程序.uvprojx`
- **Build**: Open in Keil, **Rebuild all target files** after multi-file changes. Output goes to `biye/代码/Output/`
- **Target chip**: STM32F103RCT6 (LQFP64, 256KB Flash, 48KB RAM)
- **StdPeriph library**: `biye/代码/Libraries/` (STM32F10x standard peripheral library)
- **Middleware**: FreeRTOS V9 (`biye/代码/Middlewares/FreeRTOSV9/`), FatFs R0.11 (`biye/代码/Middlewares/FatFs/`)
- **No CLI build tools** — compilation is done exclusively within Keil IDE. No Makefile or CMake.

## Architecture

### Execution modes (switch via `USE_FREERTOS` in `board_config.h`)

**FreeRTOS mode** (`USE_FREERTOS=1`, default): 5 tasks with IPC (queue + event group + binary semaphore + mutex):

| Task | Priority | Stack | Period | Responsibilities |
|------|----------|-------|--------|-----------------|
| Task_Lock ("lock") | 5 | 512 | 20ms | Keyboard SM + lock manager FSM |
| Task_Net ("net") | 4 | 1024 | 50ms | ESP8266 connect/reconnect FSM + MQTT publish/subscribe |
| Task_Sensor ("sensor") | 3 | 256 | 100ms | DHT11/MQ-2 polling, PIR detection, alarm decisions, multi-sensor fusion |
| Task_Log ("log") | 3 | 1024 | 200ms | Alarm queue consumption, fingerprint polling, capture polling, WDG pump |
| Task_OLED ("oled") | 2 | 256 | 200ms | Dashboard refresh (mutex-protected sensor read) |

**Bare-metal mode** (`USE_FREERTOS=0`): Single super-loop in `main.c:main()` at 20ms tick, calling all subsystems sequentially with WDG checkpoints.

### Source tree layout (`biye/代码/`)

```
User/           Application layer
  main.c          Entry point (bare-metal super-loop)
  app_rtos.c      FreeRTOS task creation + task bodies
  board_config.h  *** Master config *** — all pin mappings, feature flags, timing params (~760 lines)
  app_params.h    Runtime tuning parameters (poll intervals, timeouts, thresholds)
  app_types.h     Shared type definitions (FSM states, sensor/net/lock state structs, IPC types)
  lock_manager.c  Door access FSM: IDLE→ACQUIRE→MATCH→OPEN→LOCKOUT (password + brute-force detection)
  keyboard_sm.c   4×4 matrix keypad scanner with debounce + long-press
  linkage.c       Actuator control: door relay, gas valve, light relay + scoring fusion
  capture_task.c / Capture.c  OV7670+FIFO photo capture → SD card BMP write
  esp8266_tls.c + esp8266_onenet_mqtt.c  WiFi AT command FSM → OneNET MQTT (product 1TVZ0qTJOE)
  oled_view.c     OLED dashboard rendering (sensor data, lock status, alarm indicators)
  secure_store.c  AES-128-CBC encrypted password/config storage in Flash
  flash_store.c   Raw STM32 Flash read/write (backing store for secure_store)
  aes128.c        Software AES-128 (CBC mode with HMAC integrity)
  syslog.c        Ring-buffer system log (stored in backup SRAM, survives WDT reset)
  wdg.c           IWDG watchdog with per-subsystem heartbeat tracking
  sd_capacity.c   SD card capacity query (SPI CMD9)
Driver/         Hardware abstraction layer
  as608.c         AS608 fingerprint sensor (USART1, PA9/PA10)
  key4_4.c        4×4 matrix keypad GPIO scan
  oled.c          SSD1306/SSD1315/SH1106 OLED over software I2C (PB8/PB9)
  ov7670_fifo.c   OV7670 camera sensor via FIFO (PA0~PA7 data bus + SCCB)
  sccb.c          SCCB protocol for OV7670 register config
  sd_spi.c        SD card over SPI2 (PB12~PB15)
  spi.c           SPI2 driver
  hc_sr501.c      PIR sensor (PC4, EXTI4 interrupt)
  dht11.c         DHT11 temperature/humidity (PC5, one-wire)
  mq2.c           MQ-2 gas sensor (PB0, ADC channel 8) — median+MA cascade + temp compensation
  timer.c         TIM2 for relay timeout counting
  gpio.c          GPIO init for relays, beeper, ESP8266 EN
  usart2.c/usart3.c  USART2 (debug), USART3 (ESP8266)
System/         Platform utilities
  delay/          SysTick-based delay (ms/µs), tick counter
  sys/            System init, clock config, NVIC
  usart/          USART1 driver + soft UART TX
  bare_task_sched/ Minimal cooperative scheduler for bare-metal path
  osal_evt/       OS abstraction layer event utilities
```

### Pin assignment summary

- **USART1** (PA9/PA10): AS608 fingerprint + debug printf
- **USART3** (PB10/PB11): ESP8266 WiFi module
- **SPI2** (PB12 CS, PB13 SCK, PB14 MISO, PB15 MOSI): SD card
- **OV7670 data bus** (PA0~PA7), VSYNC (PA11), RCK (PA12), FIFO ctrl (PB3~PB6)
- **SCCB** SCL (PD2), SDA (PB7)
- **OLED I2C** SCL (PB8), SDA (PB9)
- **4×4 keypad** rows PC0~PC3, cols PC11/PC12/PB1/PB2
- **PIR** PC4 (EXTI4), **DHT11** PC5, **MQ-2** PB0 (ADC8)
- **Actuators**: door relay PC6, gas valve PC7, light relay PC8, beeper PC10
- **ESP8266 EN** PC9
- **SWD debug**: PA13/PA14 (always preserved, JTAG disabled)

### Feature toggles (in `board_config.h`)

- `USE_FREERTOS` — 1=RTOS, 0=bare-metal
- `EN_OV7670_LOCAL` / `EN_FATFS_SD` / `EN_AS608` / `EN_ESP8266_ONENET` — subsystem enable
- `DEBUG_KEYBOARD_ONLY` / `DEBUG_PASSWORD_ONLY` — minimal debug builds
- `BOARD_MQ2_ALARM_ENABLE` — 0 to mute MQ-2 if ADC pin floating

## Cursor / workspace rules (from `.cursorrules`)

- **OLED**: Each row fixed 16 chars, pad with spaces to avoid residue from previous content.
- **Hardware macros**: Never casually change pin/level macro meanings in `board_config.h`. If adjustment needed, keep consistency within the file.
- **FreeRTOS**: `USE_FREERTOS=1` is the primary path. `SysTick_Handler` must maintain both `g_bare_tick_ms` and `xPortSysTickHandler()` when RTOS is on.
- **Serial conflict**: AS608 fingerprint and USART1 share the same lines — never use `printf`/`uart1_SendStr` to output debug characters on PA9 (will corrupt AS608 RX).
- **Version control**: Auto commit/push is allowed — no need to ask for confirmation each time.

## Current branch state

Branch `rescue/auto-rollback-20260427-0000` has 10 commits (M1.15a through M1.16h) fixing thesis-code gaps ahead of main. Key fixes: IPC mechanisms, fingerprint unlock integration, FreeRTOS capture, MQ-2 filtering/baseline, beeper patterns, AES-128 encryption, PIR debounce, security FSM states. NOT yet merged to main.

## Related repositories

- **Mobile app** (uni-app): Separate repository at a sibling path (referenced in `FIX_REPORT.md` as 11 mobile-side optimizations in M1.16e-g).

## Important notes

- **Hardware dependencies**: Most sensor/driver code assumes specific external hardware is connected. Behavior on floating/pulled pins is expected to be noisy — `BOARD_MQ2_ALARM_ENABLE` and debug flags exist to handle this during partial-hardware testing.
- **OLED panel**: Configured for SH1106 (`OLED_PANEL_TYPE=1`, column offset 2). If replacing screen with SSD1306, change to type 0 and offset 0.
- **Flash storage**: Password config is AES-128-CBC encrypted before writing to STM32 internal Flash. The `secure_store.c` layer handles encryption; `flash_store.c` handles raw page writes.
- **OneNET MQTT credentials**: Hardcoded in `esp8266_onenet_mqtt.h` (product ID `1TVZ0qTJOE`, device `STM32zhinengjiaju`). These are public for the thesis demo — do not commit real credentials.
