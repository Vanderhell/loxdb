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

Lifecycle contract:

- successful `lox_init` performs one main core heap allocation;
- normal core operations allocate nothing afterward;
- `lox_deinit` releases the heap;
- port and user-callback allocation is outside this guarantee.

`lox_preflight` and `lox_init` share the same normalized RAM and storage-layout
calculation. `lox_preflight_report_t.storage_required_bytes` is the exact
required persistent capacity for the supplied configuration.

With `LOX_ENABLE_WAL=0`, `wal_offset`, `wal_size`, WAL statistics, and WAL
effective-capacity/admission fields are zero. No WAL region is reserved.
Persistent no-WAL builds continue to use dual snapshot banks, but mutation
durability is weaker than in WAL-enabled profiles because recovery cannot replay
an append-only mutation record.

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

Relational caller buffers use explicit lengths and capacities. For
`LOX_COL_STR`, input lengths count bytes before the terminating NUL; the input
itself need not be NUL-terminated, and the length must be smaller than the
fixed schema width so LOXDB can append the terminator. BLOB and scalar inputs
must exactly match the schema width. Indexed lookup and delete keys follow the
same rules and are normalized into an internal fixed-width key before use.

`lox_row_get(..., out, out_capacity, out_len)` and
`lox_rel_find_by(..., out_buf, out_capacity, out_len)` report the required
column or row size through `out_len`. If the capacity is too small they return
`LOX_ERR_OVERFLOW` without modifying the destination.

Migration from versions before this change requires adding `val_len` to
`lox_row_set`, `search_len` to indexed find/delete operations, and explicit
output capacities to row getters and `lox_rel_find_by`.

Schema-version transitions require identical physical schemas. Any column add,
remove, rename, resize, reorder, or reindex returns `LOX_ERR_SCHEMA`.

## Persistence and mutation failures

The WAL header is immutable within a generation and entries append after it.
Reset/compaction is the only WAL erase path; replay stops at the first invalid
tail. `LOX_WAL_SYNC_ALWAYS` syncs mutations, while
`LOX_WAL_SYNC_FLUSH_ONLY` defers the sync boundary.

Failures known to occur before mutation begins use deterministic errors. A
storage failure after mutation may have begun returns
`LOX_ERR_INDETERMINATE`, faults the handle, and blocks later
mutation/flush/compact calls. Deinitialize and reopen before continuing.

The current persistent formats serialize TS timestamps and KV expiration as
unsigned 64-bit values, independent of the configured in-memory timestamp
width, and retain compatibility with legacy 32-bit values. Reopen returns
`LOX_ERR_OVERFLOW` when a stored value does not fit the configured
`lox_timestamp_t`; this width mismatch is not reported as corruption and is
never truncated. Unsupported format versions remain a distinct error.

Persistent compatibility is currently restricted to hosts with the same native
byte order: WAL, page, and superblock integer fields use the host byte order.
Moving media between little-endian and big-endian hosts is unsupported until a
future versioned format migration defines canonical byte order.
See [Persistent Byte Order](PERSISTENCE_BYTE_ORDER.md) for the serialized-field
inventory and the required versioned migration plan.

## Verification Status

- Installed consumer usage is gated by detached build tests in this repository.
- Hardware-specific validation status is tracked separately in `docs/EVIDENCE_MATRIX.md`.
