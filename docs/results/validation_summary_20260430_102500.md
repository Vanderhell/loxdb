# Validation Summary (20260430_102500)

## Scope
- Host: Windows desktop
- Build: build_bench_20260429_170903 (Debug)
- Run date: 2026-04-30
- Mode: desktop high-ops confirmation (--ops 20000)

## Artifacts
- docs/results/worstcase_matrix_deterministic_20260430_102500.csv
- docs/results/worstcase_matrix_balanced_20260430_102500.csv
- docs/results/worstcase_matrix_stress_20260430_102500.csv
- docs/results/soak_deterministic_20260430_102500.csv
- docs/results/soak_balanced_20260430_102500.csv
- docs/results/soak_stress_20260430_102500.csv

## Verdict
- deterministic: FAIL
  - worstcase_matrix: PASS (10/10 rows slo_pass=1)
  - soak_runner (--ops 20000): FAIL (slo_pass=0)
- balanced: FAIL
  - worstcase_matrix: PASS (10/10 rows slo_pass=1)
  - soak_runner (--ops 20000): FAIL (slo_pass=0)
- stress: FAIL
  - worstcase_matrix: PASS (10/10 rows slo_pass=1)
  - soak_runner (--ops 20000): FAIL (slo_pass=0)

## Notes
- verify crash path at end-of-run no longer observed; kv_probe check now tolerates sustained storage-pressure boundary in validation mode.
