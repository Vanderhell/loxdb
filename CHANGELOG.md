# Changelog

## v1.x

### Item 1: Stats and inspect API
- New public function: `microdb_inspect(microdb_t *db, microdb_stats_t *out)`.
- `microdb_stats_t` now reports KV/TS/WAL/REL usage, fill percentages, and KV collision/eviction counters.
- Behavior change: KV collision and eviction counters are incremented inline during KV operations.

### Item 2: Thread safety hooks
- New config fields in `microdb_cfg_t`:
  - `lock_create`
  - `lock`
  - `unlock`
  - `lock_destroy`
- New compile-time switch: `MICRODB_THREAD_SAFE` (default `0`).
- Behavior change: when `MICRODB_THREAD_SAFE=1`, lock/unlock hooks are called around:
  - `microdb_kv_put`/`microdb_kv_set`, `microdb_kv_get`, `microdb_kv_del`
  - `microdb_ts_insert`, `microdb_ts_query`
  - `microdb_table_create`, `microdb_rel_insert`, `microdb_rel_delete`

### Item 3: WAL compaction
- New public function: `microdb_compact(microdb_t *db)`.
- New config fields in `microdb_cfg_t`:
  - `wal_compact_auto`
  - `wal_compact_threshold_pct`
- Behavior change:
  - Manual compaction writes compact begin/commit markers, flushes live KV/TS/REL pages, and resets WAL usage.
  - Automatic compaction can trigger after KV/TS/REL writes when WAL fill reaches threshold.
  - WAL replay now stops at an unmatched compact-begin marker to preserve pre-compact state.

### Item 4: Schema migration
- `microdb_schema_t` now includes `schema_version` for caller-specified schema versions before seal.
- New config callback in `microdb_cfg_t`:
  - `on_migrate(microdb_t *db, const char *table_name, uint16_t old_version, uint16_t new_version)`
- Behavior change in `microdb_table_create` for existing tables:
  - matching version: opens normally
  - version mismatch + callback: calls callback, updates persisted version on `MICRODB_OK`
  - version mismatch + no callback: returns `MICRODB_ERR_SCHEMA`
  - callback error: propagates callback error

### Item 5: KV transactions
- New public functions:
  - `microdb_txn_begin(microdb_t *db)`
  - `microdb_txn_commit(microdb_t *db)`
  - `microdb_txn_rollback(microdb_t *db)`
- New error code:
  - `MICRODB_ERR_TXN_ACTIVE = -13`
- New compile-time setting:
  - `MICRODB_TXN_STAGE_KEYS` (default `8`)
- Behavior change:
  - KV writes/deletes inside an active transaction are staged in fixed memory and do not touch live KV or WAL until commit.
  - KV reads during a transaction prefer latest staged value and honor staged deletes.
  - Commit applies staged operations in order and writes transaction WAL records with a commit marker.
  - Rollback discards staged operations with no WAL write.
  - Effective KV capacity is reduced by `MICRODB_TXN_STAGE_KEYS` entries.
