# Acceptance Evidence Index (One Page)

## Branch and Freeze

- Working branch: `feature/thesis-align-20260422`
- Freeze commit: `8b7d20a9` (`docs: add thesis alignment delivery report`)

## Milestone Tags

- `v0.9-thesis-align-core`
  - Core P0 closure (network/sd/sensor/actuator/alignment baseline)
- `v1.0-thesis-align-stable`
  - Points to `82a417da` (`feat(p1): unify tagged logs and centralize runtime params`)
- `v1.1-capture-sd-retention`
  - Points to `8b7d20a9` (current acceptance freeze)

## Key Commits for Review

- `82a417da` - unified tagged logs + centralized runtime parameters
- `dab45189` - stable script-based smart build path
- `d797f803` - structured `STAT_EXPORT` runtime JSON metrics
- `c16bbde4` - Chapter 6 minimal repro doc + extraction tooling
- `8b7d20a9` - full delivery report and acceptance mapping

## Build and Artifact Evidence

- Build log:
  - `biye/代码/Output/build.log`
- Verification scripts:
  - `.vscode/smart_build.ps1`
  - `.vscode/post_build_verify.ps1`
- Verified artifact hashes (latest checked):
  - AXF: `D7E28DA8CD54A291E9E6305930BDB71714B37C64B7B4997FB2F27DB5943993EB`
  - HEX: `A168441F3B428E27569E9E383BE5B08C0440C9DA19C9805D877A71722A08D4AA`

## Runtime Metric Evidence (Chapter 6)

- Firmware structured export line:
  - `{"tag":"STAT_EXPORT","tick":...,"false_alarm":...,"reconnect":...,"replay":...,"sd_fail":...,"run_ex_72h":...}`
- Extraction script:
  - `.vscode/extract_stat_export.ps1`
- Repro instruction:
  - `biye/ch6_test_repro_minimal.md`

## Documentation Entry Points

- Delivery report:
  - `biye/thesis_align_delivery_report.md`
- Requirement baseline:
  - `biye/代码完善提示词_含版本管理.md`

