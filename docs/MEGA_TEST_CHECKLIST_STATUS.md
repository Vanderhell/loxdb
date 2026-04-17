# Mega Test Checklist Status

Source plan: `bench/microdb_esp32_s3_bench/MEGA_BENCH_TEST_PLAN.md`

## Summary

- Current `MDB_TEST(...)` count target after this update: `300+`.
- Existing coverage is strongest in: KV, TS, REL, WAL replay/corruption, migration basics, txn replay/idempotence, reopen/persistence, profile matrix.
- Main historical gaps were: explicit API misuse/contract matrix and broader mixed random workload combinations.

## Section Status

### 1. Boot / open / close / reopen
- Status: Mostly covered
- Covered by: `test_integration.c`, `test_wal.c`, `test_compact.c`, `test_durability_closure.c`, `test_resilience.c`
- Remaining: explicit storage-driver forced read/write error injection on open path

### 2. KV basic
- Status: Covered
- Covered by: `test_kv.c`, `test_kv_reject.c`, `test_integration.c`

### 3. KV correctness / edge cases
- Status: Covered (high)
- Covered by: `test_kv.c`, `test_kv_reject.c`, `test_wal.c`, `test_integration.c`

### 4. TS basic
- Status: Covered
- Covered by: `test_ts.c`, `test_ts_reject.c`, `test_ts_downsample.c`, `test_integration.c`

### 5. TS ordering / retention / correctness
- Status: Covered (high)
- Covered by: `test_ts.c`, `test_ts_reject.c`, `test_ts_downsample.c`, `test_integration.c`

### 6. REL basic
- Status: Covered
- Covered by: `test_rel.c`, `test_integration.c`, `test_wal.c`

### 7. REL indexy / constraints / correctness
- Status: Covered (high)
- Covered by: `test_rel.c`, `test_wal.c`, `test_integration.c`
- Remaining: explicit deep corruption detection for synthetic broken REL row/index payloads

### 8. WAL / journaling
- Status: Covered (high)
- Covered by: `test_wal.c`, `test_wal_kv128.c`, `test_wal_no_wal.c`, `test_compact.c`

### 9. Transactions
- Status: Covered (high)
- Covered by: `test_txn.c`, `test_durability_closure.c`, `test_wal.c`

### 10. Migration / schema
- Status: Covered (medium-high)
- Covered by: `test_migration.c`, `test_integration.c`
- Remaining: explicit multi-step migration chains and downgrade/future-version rejection matrix

### 11. Corruption / recovery / power-fail
- Status: Covered (high)
- Covered by: `test_wal.c`, `test_durability_closure.c`, `test_compact.c`, `test_resilience.c`

### 12. Capacity / limits / stress
- Status: Covered (high)
- Covered by: `test_limits.c`, `test_integration.c`, `test_resilience.c`

### 13. API / contract / misuse
- Status: Covered (high) after contract matrix extension
- Covered by: `test_kv.c`, `test_ts.c`, `test_rel.c`, `test_integration.c`, `test_resilience.c`, `test_api_contract_matrix.c`

### 14. Stats / inspect / observability
- Status: Covered
- Covered by: `test_stats.c`, `test_integration.c`, `test_wal.c`, `test_resilience.c`

### 15. Performance regression tests
- Status: Partially covered
- Covered by: ESP32 bench (`bench/microdb_esp32_s3_bench`) with SLO + latency output
- Remaining: host-side CI assertions with fixed latency envelopes

## Next Recommended Gaps

- Storage backend fault-injection hooks for open/read/write/erase/sync error matrix.
- REL corruption-injection tests (broken row/index records).
- CI-friendly perf guardrails (soft thresholds with profile-specific variance windows).
