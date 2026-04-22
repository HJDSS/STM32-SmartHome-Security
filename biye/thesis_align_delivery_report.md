# Thesis Align Delivery Report

This document summarizes the implementation alignment work for `biye` against thesis Chapters 3/4/6 and `board_config.h` hardware facts.

## 1) Difference Analysis (Current vs Target)

- `main.c` was previously incomplete and did not provide a stable super-loop orchestration for sensors, alarm linkage, network lifecycle, and capture pipeline.
- ESP/SD/sensor/actuator exception paths were partially available but lacked complete offline-run, reconnect idempotency, and runtime observability closure.
- Build reproducibility in VS Code had quoting/path fragility and weak post-build evidence output in localized path environments.
- Chapter 6 metric extraction had plain-text counters only; no stable structured output and no minimum reproducible extraction workflow.

## 2) Implementation Plan and Completion (P0/P1/P2)

- P0 completed:
  - ESP-01S link alignment and reconnect handling.
  - SD mount + OV7670 capture local retention + deferred publish replay.
  - DHT/MQ2/PIR behavior alignment and key fault/recover logs.
  - Actuator safe-state path at init/recovery.
- P1 completed:
  - Unified tagged logs (`NET/SD/SENSOR/ACT/STAT`) with macro switches.
  - Centralized tunable runtime parameters in one header.
- P2 completed:
  - Structured statistic export line (`STAT_EXPORT` JSON).
  - Minimal Chapter 6 repro note and extraction script (`CSV/JSONL`).

## 3) Actual Changed Files (Representative)

- Core loop and runtime behavior:
  - `biye/代码/User/main.c`
  - `biye/代码/User/esp8266_tls.c`
  - `biye/代码/User/Capture.c`
  - `biye/代码/Driver/gpio.c`
  - `biye/代码/Driver/gpio.h`
- P1 architecture and diagnostics:
  - `biye/代码/User/log.h`
  - `biye/代码/User/app_params.h`
- Build/test reproducibility:
  - `.vscode/tasks.json`
  - `.vscode/post_build_verify.ps1`
  - `.vscode/smart_build.ps1`
  - `.vscode/extract_stat_export.ps1`
  - `biye/ch6_test_repro_minimal.md`

## 4) Commit Summary

- `feat(net/sd/sensor/actuator)`-series and baseline commits rebuilt and aligned the executable chain to thesis and hardware constraints.
- `82a417da` `feat(p1): unify tagged logs and centralize runtime params`
- `dab45189` `chore: add PowerShell smart build script`
- `d797f803` `feat(p2): add structured runtime stat export`
- `c16bbde4` `docs(p2): add chapter-6 minimal repro tooling`
- Milestone tag:
  - `v0.9-thesis-align-core` (previously created after P0 closure)
  - `v1.0-thesis-align-stable` (points to `82a417da`, P1 completion)

## 5) Verification Results and Remaining Items

- Build and verify:
  - `build.log` reports `0 Error(s), 0 Warning(s)`.
  - Artifact verification script outputs SHA256 for `axf/hex`.
- Functional closure:
  - Capture path supports local SD retention and network-online publish/deferred replay.
  - Runtime counters now include false alarm, reconnect, replay, SD write failure, and long-run exception.
- Remaining item:
  - One untracked pre-existing directory with garbled display name remains in working tree; not modified in this implementation flow.

## 6) Next Recommendations

- Run one full Chapter 6 batch and archive:
  - Build hash evidence
  - UART `STAT_EXPORT` log
  - Extracted `CSV/JSONL`
- Optionally add `v1.1-capture-sd-retention` tag at the commit you use for final acceptance freeze.
- If needed, add a tiny host-side plot script for trend visualization from `stat_export.csv` (non-firmware impacting).

