# API Reference

This page is a compact reference for the public surfaces shipped by `loxdb`.
For implementation details, use `include/lox.h`, `include/lox_cpp.hpp`, and the
module sources.

## Public Headers

- `lox.h`: core C API
- `lox_cpp.hpp`: C++17 wrapper
- `lox_config.h`: installed configuration identity header
- `lox_capacity_profile.h`: capacity/profile helpers
- `lox_json_wrapper.h`: JSON wrapper module
- `lox_import_export.h`: import/export module
- `lox_backend_*.h`: backend adapter/orchestration helpers
- `loxdb/port/lox_port_ram.h`: RAM test port
- `loxdb/port/lox_port_posix.h`: POSIX port

## Core Surface

- `lox_init`, `lox_deinit`, `lox_flush`
- `lox_preflight`
- `lox_stats`, `lox_inspect`, `lox_get_db_stats`, `lox_get_kv_stats`, `lox_get_ts_stats`, `lox_get_rel_stats`
- `lox_get_effective_capacity`, `lox_get_pressure`, `lox_selfcheck`
- `lox_admit_kv_set`, `lox_admit_ts_insert`, `lox_admit_rel_insert`
- `lox_compact`

## KV

- `lox_kv_set`, `lox_kv_put`, `lox_kv_get`, `lox_kv_del`, `lox_kv_exists`
- `lox_kv_iter`, `lox_kv_purge_expired`, `lox_kv_clear`

## Transactions

- `lox_txn_begin`, `lox_txn_commit`, `lox_txn_rollback`

## TS

- `lox_ts_register`, `lox_ts_register_ex`
- `lox_ts_insert`, `lox_ts_last`, `lox_ts_query`, `lox_ts_query_buf`, `lox_ts_count`, `lox_ts_clear`

## REL

- `lox_schema_init`, `lox_schema_add`, `lox_schema_seal`
- `lox_table_create`, `lox_table_get`, `lox_table_row_size`
- `lox_row_set`, `lox_row_get`
- `lox_rel_insert`, `lox_rel_find`, `lox_rel_find_by`, `lox_rel_delete`, `lox_rel_iter`, `lox_rel_count`, `lox_rel_clear`

## Verification Status

- Installed consumer usage is gated by detached build tests in this repository.
- Hardware-specific validation status is tracked separately in `docs/EVIDENCE_MATRIX.md`.
