# Profile Guarantees and Limits

This document is the contract-level summary of what the current codebase guarantees.

## Scope

- Source of truth: `include/microdb.h`, `CMakeLists.txt`, and test coverage in `tests/`.
- This document intentionally avoids historical benchmark numbers.
- Runtime metrics in `docs/results/` are snapshots, not API contract.

## Supported compile-time core profiles

Exactly one profile macro can be enabled:

- `MICRODB_PROFILE_CORE_MIN`
- `MICRODB_PROFILE_CORE_WAL`
- `MICRODB_PROFILE_CORE_PERF`
- `MICRODB_PROFILE_CORE_HIMEM`
- `MICRODB_PROFILE_FOOTPRINT_MIN`

If none is set, `MICRODB_PROFILE_CORE_WAL` is selected by default.

## Durable storage contract (current releases)

Validated at `microdb_init()` / open path:

- `erase_size > 0`
- `write_size == 1`

If violated, initialization fails with `MICRODB_ERR_INVALID`.

## Engine guarantees by build/profile

- KV: available when `MICRODB_ENABLE_KV=1`.
- TS: available when `MICRODB_ENABLE_TS=1`.
- REL: available when `MICRODB_ENABLE_REL=1`.
- WAL durability path: available when `MICRODB_ENABLE_WAL=1` and storage HAL is provided.

`MICRODB_PROFILE_FOOTPRINT_MIN` contract:

- KV enabled
- TS disabled
- REL disabled
- WAL enabled

`microdb_tiny` variant (CMake target) is not equivalent to footprint-min durability:

- TS disabled
- REL disabled
- WAL disabled

## Error contract

Stable public error enum is `microdb_err_t`.
String mapping helper:

- `microdb_err_to_string(microdb_err_t)`

Detailed semantics:

- `docs/FAIL_CODE_CONTRACT.md`

## Recovery and verification

- Recovery behavior and power-loss expectations are validated by WAL/recovery tests in `tests/`.
- Offline image verification contract is documented in `docs/OFFLINE_VERIFIER.md`.

## What this document does not guarantee

- absolute latency numbers
- platform-independent throughput
- fixed retention counts for every custom macro combination

Those must be validated on the target hardware/workload.

## Explicit release statements

- Effective capacity is runtime/layout-dependent and is not equal to raw/target capacity.
- Stress profile is a throughput/pressure profile and is not a low-latency profile.
- Deterministic profile is the profile for controlled latencies.
- `reopen` and `compact` are maintenance operations and are not normal write-path latency.

## Latest validated snapshots

- Desktop full validation (all profiles PASS): `docs/results/validation_summary_20260419_180500.md`
- Consolidated verdict/report: `docs/results/hard_verdict_20260419.md`
