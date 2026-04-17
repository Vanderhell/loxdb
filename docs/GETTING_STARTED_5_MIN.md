# Getting Started (5 min)

This is the fastest path to a working persistent `microdb` integration.

## 1. Add the library

```cmake
add_subdirectory(microdb)
target_link_libraries(your_app PRIVATE microdb)
```

## 2. Provide storage HAL (or start RAM-only)

- RAM-only:
  - set `cfg.storage = NULL`
  - useful for feature bring-up
- persistent mode:
  - provide `read/write/erase/sync` callbacks in `microdb_storage_t`
  - set correct `capacity`, `erase_size`, `write_size`
  - current storage contract: `erase_size > 0`, `write_size == 1` (other `write_size` values fail init)

## 3. Initialize with explicit profile-like budget

```c
#include "microdb.h"

static microdb_t db;
static microdb_storage_t st; /* fill if persistent */

microdb_cfg_t cfg = {0};
cfg.storage = &st;   /* or NULL for RAM-only */
cfg.ram_kb = 64;     /* start with balanced-like budget */
cfg.kv_pct = 40;
cfg.ts_pct = 40;
cfg.rel_pct = 20;

microdb_err_t rc = microdb_init(&db, &cfg);
```

## 4. Basic API flow

```c
/* KV */
uint32_t v = 123;
microdb_kv_put(&db, "boot_count", &v, sizeof(v));

/* TS */
microdb_ts_register(&db, "temp", MICRODB_TS_U32, 0);
microdb_ts_insert(&db, "temp", 1, &v);

/* REL */
microdb_schema_t s;
microdb_table_t *t = NULL;
microdb_schema_init(&s, "devices", 32);
microdb_schema_add(&s, "id", MICRODB_COL_U32, sizeof(uint32_t), true);
microdb_schema_add(&s, "state", MICRODB_COL_U8, sizeof(uint8_t), false);
microdb_schema_seal(&s);
microdb_table_create(&db, &s);
microdb_table_get(&db, "devices", &t);
```

## 5. Shutdown / durability points

- call `microdb_flush(&db)` at controlled checkpoints
- call `microdb_compact(&db)` in maintenance windows
- call `microdb_deinit(&db)` on clean shutdown

## 6. Read contracts before release

- fail codes: `docs/FAIL_CODE_CONTRACT.md`
- profile guarantees: `docs/PROFILE_GUARANTEES.md`
- hard verdict report: `docs/results/hard_verdict_20260412.md`

## 7. ESP32 bench quick check

Use the terminal bench and run:
- `run_det`
- `profile balanced` + `run`
- `profile stress` + `run`

Then check `metrics` and `config` output.
