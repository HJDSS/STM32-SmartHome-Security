# Chapter 6 Minimal Repro (P2)

This note provides a minimal and repeatable test path for thesis Chapter 6 metrics.

## 1) Build and artifact verification

1. Run:
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\.vscode\smart_build.ps1`
2. Confirm:
   - `biye/代码/Output/build.log` shows `0 Error(s), 0 Warning(s)`.
   - SHA256 lines are printed by `post_build_verify.ps1`.

## 2) Runtime metric export points

Firmware now outputs two statistic lines on UART1:

- Human-readable:
  - `[STAT] false_alarm=... reconnect=... replay=... sd_fail=... run_ex=...`
- Structured JSON (for scripts):
  - `{"tag":"STAT_EXPORT","tick":...,"false_alarm":...,"reconnect":...,"replay":...,"sd_fail":...,"run_ex_72h":...}`

Required Chapter 6 fields are mapped as:

- `false_alarm` -> false alarm counter
- `reconnect` -> reconnect count
- `replay` -> deferred-upload replay count
- `sd_fail` -> SD write failure count
- `run_ex_72h` -> long-run exception counter

## 3) Data extraction from serial log

1. Save UART1 log to a text file, for example:
   - `D:\logs\uart_session.txt`
2. Extract structured metrics:
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\.vscode\extract_stat_export.ps1 -InputPath "D:\logs\uart_session.txt"`
3. Output files are generated beside input (or custom paths):
   - `*.stat_export.csv`
   - `*.stat_export.jsonl`

## 4) Minimal scenario matrix

1. Normal online run (baseline)
   - Expect `reconnect` stable, `sd_fail` near zero.
2. Network drop and recovery
   - Force AP disconnect then restore.
   - Expect `reconnect` and/or `replay` increase.
3. PIR false trigger in disarmed mode
   - Trigger PIR while disarmed.
   - Expect `false_alarm` increase.
4. Long run (target 72h)
   - Continuous power/network disturbances as needed.
   - Track `run_ex_72h` trend from exported data.

## 5) Acceptance snapshot

For report screenshots/tables, keep:

- Build result (`build.log` + hash output)
- At least one JSON `STAT_EXPORT` line
- Extracted CSV/JSONL with timestamped test batch

