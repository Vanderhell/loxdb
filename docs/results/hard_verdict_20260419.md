# Hard Verdict Report (2026-04-19)

## Product Identity
microdb is a deterministic durable storage core for MCU/embedded systems with power-fail-safe WAL recovery.

## Desktop (full matrix + soak)
Source:
- `docs/results/validation_summary_20260419_152258.md`
- `docs/results/worstcase_matrix_*_20260419_152258.csv`
- `docs/results/soak_*_20260419_152258.csv`

Verdict:
- deterministic: PASS
  - matrix_slo=PASS soak_slo=PASS fail_count=0
  - max_kv_put=29550us max_ts_insert=29425us max_rel_insert=28765us max_rel_delete=42875us
  - max_txn_commit=32251us max_compact=24951us max_reopen=55646us
- balanced: PASS
  - matrix_slo=PASS soak_slo=PASS fail_count=0
  - max_kv_put=36835us max_ts_insert=38000us max_rel_insert=37995us max_rel_delete=36772us
  - max_txn_commit=39252us max_compact=31569us max_reopen=68217us
- stress: PASS
  - matrix_slo=PASS soak_slo=PASS fail_count=0
  - max_kv_put=45400us max_ts_insert=45675us max_rel_insert=47053us max_rel_delete=43724us
  - max_txn_commit=48209us max_compact=40419us max_reopen=86064us

## ESP32-S3 (real HW)
Latest available sources:
- `docs/results/validation_summary_20260412_203442.md`
- `docs/results/esp32_stress_20260412_205405.log`

Status:
- deterministic: PASS (2026-04-12)
- balanced: PASS (2026-04-12)
- stress: PASS (2026-04-12)
- Note: no fresh ESP run on 2026-04-19 in this verification pass.

## Current Overall Verdict
- Core quality: strong and validated on latest desktop full matrix + soak across all three profiles.
- Release readiness: desktop gates are green; final release GO still requires fresh ESP validation on current firmware.
