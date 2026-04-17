# Profile Guarantees and Limits

This table is the product-facing contract for current profiles.

## Scope
- Numbers are based on latest validation artifacts from `2026-04-12`.
- Desktop values come from `docs/results/validation_summary_20260412_214613.md`.
- ESP values come from:
  - `docs/results/esp32_deterministic_20260412_203442.log`
  - `docs/results/esp32_balanced_20260412_203442.log`
  - `docs/results/esp32_stress_20260412_205405.log`
- `run_det` validates deterministic profile latency only, not all profiles/workloads.

## Contract Table

| Profile | Goal | Guarantees | Does Not Guarantee | Typical Latency (ESP p50) | Worst-Case Latency (latest ESP run) | Reopen Time (latest ESP run) | Effective Capacity (ESP) |
|---|---|---|---|---|---|---|---|
| deterministic | Tight latency and predictable behavior | benchmark checks pass, migration/txn checks pass, low tail under deterministic workload | best latency under stress-like data volume | KV put/get/del ~67/9/99 us, TS insert ~21 us, REL insert ~29 us, WAL put ~75 us | max op ~10.9 ms (compact excluded), compact ~10.9 ms | ~21.4 ms | kv_capacity=376 (target=192), wal_total=32736B |
| balanced | Throughput + stability compromise | benchmark checks pass, SLO checks pass in balanced envelope | deterministic worst-case bounds | KV put/get/del ~68/9/101 us, TS insert ~21 us, REL insert ~32 us, WAL put ~79 us | max op ~9.5 ms (ts_insert), compact ~14.9 ms | ~56.7 ms | kv_capacity=376 (target=320), wal_total=32736B |
| stress | High data pressure and capacity burn | benchmark checks pass, stress SLO checks pass, robust open/run after storage-layout fix | low tail latency / hard deterministic bounds | KV put/get/del ~68/9/102 us, TS insert ~20 us, REL insert ~182 us, WAL put ~87 us | max op ~18.6 ms (wal_kv_put), compact ~21.6 ms | ~301.2 ms | kv_capacity=376 (target=900), wal_total=8160B |

## Effective Capacity Transparency
- `target_*` values are workload targets for benchmark pressure.
- `kv_capacity` is compile/layout-limited effective capacity.
- `target > effective` means workload is intentionally pressuring limits, not promising retained capacity.

## Recommendation
- Use `deterministic` for latency-sensitive control loops.
- Use `balanced` for general embedded products.
- Use `stress` for endurance/capacity validation and soak, not as a deterministic latency profile.
