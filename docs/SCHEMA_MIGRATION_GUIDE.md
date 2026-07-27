# Schema Version Transition Guide

> **Important:** `loxdb` does not implement physical schema migration. A
> version transition is supported only when the old and new schemas have an
> identical physical row layout.

## Supported contract

Set `lox_schema_t.schema_version` before `lox_schema_seal()`. The sealed
version is immutable; changing it afterward causes `lox_table_create()` to
return `LOX_ERR_SCHEMA`.

For an existing table:

- the same physical schema and version returns `LOX_OK`;
- the same physical schema with a different version requires
  `lox_cfg_t.on_migrate`;
- the callback runs outside the database lock and may transform values only
  within the existing row layout;
- a successful callback makes the new version durable before it becomes the
  in-memory version;
- a callback error leaves the old version active;
- recursive version transitions from the callback return `LOX_ERR_SCHEMA`.

Physical identity includes row count, row size, column count, column order,
column names, types, sizes, offsets, and index selection. Adding, removing,
renaming, resizing, reordering, or reindexing columns is unsupported and
returns `LOX_ERR_SCHEMA` before the callback runs, before WAL is written, and
before the in-memory version changes.

## Failure handling

A deterministic rejection or callback failure leaves both the durable and
in-memory version unchanged. If storage fails after persistence begins,
`lox_table_create()` returns `LOX_ERR_INDETERMINATE`, faults the handle, and
does not claim which version is durable. Deinitialize and reopen the database
to recover the last valid durable state before issuing more mutations.

The focused evidence is in `tests/test_migration.c`, including physical-layout
rejection, callback locking and recursion, WAL ordering, reopen, and injected
storage failures.
