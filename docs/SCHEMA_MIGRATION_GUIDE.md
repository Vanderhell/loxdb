# Schema Migration Guide

This guide explains how relational schema evolution works in `microdb`.

## 1) Migration contract

Relevant API fields:

- `microdb_schema_t.schema_version`
- `microdb_cfg_t.on_migrate`
- `microdb_table_create(...)`

Behavior when table already exists:

1. same version:
   - `microdb_table_create` returns `MICRODB_OK`
2. version mismatch and no `on_migrate` callback:
   - returns `MICRODB_ERR_SCHEMA`
3. version mismatch with `on_migrate` callback:
   - callback is called with `(table_name, old_version, new_version)`
   - if callback returns `MICRODB_OK`, new version is persisted
   - callback error is propagated

## 2) Minimal migration callback

```c
static microdb_err_t on_migrate_cb(microdb_t *db,
                                   const char *table_name,
                                   uint16_t old_version,
                                   uint16_t new_version) {
    (void)db;
    (void)table_name;

    /* TODO: read old rows, transform data, validate invariants. */
    if (old_version == 1u && new_version == 2u) {
        return MICRODB_OK;
    }
    return MICRODB_ERR_SCHEMA;
}
```

## 3) Upgrade flow example

1. Build schema v1:
   - `schema.schema_version = 1`
   - `microdb_schema_init/add/seal`
   - `microdb_table_create(...)`
2. Reopen DB with `cfg.on_migrate = on_migrate_cb`.
3. Build schema v2:
   - `schema.schema_version = 2`
   - `microdb_schema_init/add/seal`
   - `microdb_table_create(...)`
4. On success:
   - callback executed once
   - persisted table schema version updated to v2

## 4) Design recommendations

- Treat migrations as deterministic transformations.
- Make callbacks idempotent where possible.
- Validate row-level constraints before returning `MICRODB_OK`.
- Keep migration logic narrow (one version step at a time).
- Fail fast (`MICRODB_ERR_SCHEMA` or `MICRODB_ERR_INVALID`) on unsafe transforms.

## 5) Data compatibility patterns

- Add column:
  - define default value in migration callback for existing rows
- Rename/reshape column:
  - copy old value to new field explicitly in callback
- Drop column:
  - ensure dependent logic no longer reads dropped field before shipping migration

## 6) Test-backed reference

See:

- `tests/test_migration.c`

Covered scenarios:

- migration callback called on version bump
- no callback call for matching version
- version mismatch without callback returns `MICRODB_ERR_SCHEMA`
- callback error propagation

