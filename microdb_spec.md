# microdb — Embedded Database Engine for ESP32

> **C99 · Single malloc at init · Zero dependencies · Three engines · HAL-abstracted storage**
>
> A lightweight embedded database for microcontrollers. One `#include`, one `microdb_init()`,
> and you have key-value storage, time-series logging, and relational tables — all within
> a fixed RAM budget you define at compile time.

---

## Table of Contents

1. [Design philosophy](#1-design-philosophy)
2. [Repository layout](#2-repository-layout)
3. [Configuration reference](#3-configuration-reference)
4. [Memory model](#4-memory-model)
5. [Storage HAL](#5-storage-hal)
6. [Error codes](#6-error-codes)
7. [Core API — init / deinit](#7-core-api--init--deinit)
8. [KV engine](#8-kv-engine)
9. [Time-series engine](#9-time-series-engine)
10. [Relational engine](#10-relational-engine)
11. [Flush and persistence](#11-flush-and-persistence)
12. [Power-loss safety](#12-power-loss-safety)
13. [Thread safety](#13-thread-safety)
14. [RAM budget split](#14-ram-budget-split)
15. [Compile-time limits and defaults](#15-compile-time-limits-and-defaults)
16. [File layout of microdb.h](#16-file-layout-of-microdbh)
17. [Implementation notes per engine](#17-implementation-notes-per-engine)
18. [Test requirements](#18-test-requirements)
19. [Usage examples](#19-usage-examples)
20. [What microdb is NOT](#20-what-microdb-is-not)

---

## 1. Design philosophy

```
One malloc.   At microdb_init(). Never again.
One header.   #include "microdb.h" — everything is there.
One budget.   MICRODB_RAM_KB tells the library how much RAM it owns.
Zero deps.    No libc beyond memcpy/memset/malloc/free. No RTOS.
HAL storage.  microdb does not know about flash, SD, or FRAM.
              It calls function pointers you give it.
```

Every decision in this spec follows from these five rules.

If a feature requires a second malloc after init → it is not implemented.
If a feature requires knowing which flash chip is present → it goes into the HAL.
If a feature requires a heap → it is a compile-time option disabled by default.

---

## 2. Repository layout

```
microdb/
├── include/
│   └── microdb.h          ← single public header, everything the user needs
├── src/
│   ├── microdb.c          ← core: init, deinit, RAM allocator, dispatch
│   ├── microdb_kv.c       ← KV engine
│   ├── microdb_kv.h       ← KV internal types (not public)
│   ├── microdb_ts.c       ← time-series engine
│   ├── microdb_ts.h       ← TS internal types
│   ├── microdb_rel.c      ← relational engine
│   ├── microdb_rel.h      ← REL internal types
│   └── microdb_wal.c      ← write-ahead log (power-loss safety)
├── port/
│   ├── esp32/
│   │   └── microdb_port_esp32.c   ← ESP-IDF flash HAL example
│   ├── posix/
│   │   └── microdb_port_posix.c   ← POSIX file HAL for tests/simulator
│   └── ram/
│       └── microdb_port_ram.c     ← volatile RAM HAL (testing only)
├── tests/
│   ├── test_kv.c
│   ├── test_ts.c
│   ├── test_rel.c
│   ├── test_wal.c
│   ├── test_integration.c
│   └── test_limits.c
├── examples/
│   ├── esp32_sensor_node/
│   └── posix_simulator/
├── docs/
│   └── architecture.md
├── CMakeLists.txt
└── README.md
```

---

## 3. Configuration reference

All configuration is done via `#define` **before** `#include "microdb.h"`.
The library reads these defines once at compile time — no runtime configuration structs for limits.

### 3.1 RAM budget

```c
#define MICRODB_RAM_KB 32       // Total RAM microdb may malloc. Range: 8–4096.
                                // Default: 32
                                // microdb_init() does exactly one malloc(MICRODB_RAM_KB * 1024)
```

### 3.2 Engine selection

```c
#define MICRODB_ENABLE_KV   1   // Include KV engine.  Default: 1
#define MICRODB_ENABLE_TS   1   // Include TS engine.  Default: 1
#define MICRODB_ENABLE_REL  1   // Include REL engine. Default: 1
```

If an engine is disabled (set to 0):
- Its functions are excluded from compilation (guarded by `#if`)
- Its RAM slice is given to the other engines proportionally
- Calling a disabled engine's function returns `MICRODB_ERR_DISABLED`

### 3.3 RAM split ratios

Controls how MICRODB_RAM_KB is divided between engines.
Values are percentages. Must sum to 100.

```c
#define MICRODB_RAM_KV_PCT   40    // Default: 40%  — KV hash table + value store
#define MICRODB_RAM_TS_PCT   40    // Default: 40%  — TS ring buffers
#define MICRODB_RAM_REL_PCT  20    // Default: 20%  — REL table storage + schema
```

Override example — KV-heavy node:
```c
#define MICRODB_RAM_KV_PCT   70
#define MICRODB_RAM_TS_PCT   20
#define MICRODB_RAM_REL_PCT  10
```

### 3.4 KV engine limits

```c
#define MICRODB_KV_MAX_KEYS      64     // Max number of KV entries. Default: 64
#define MICRODB_KV_KEY_MAX_LEN   32     // Max key string length incl. NUL. Default: 32
#define MICRODB_KV_VAL_MAX_LEN   128    // Max value blob size in bytes. Default: 128
#define MICRODB_KV_ENABLE_TTL    1      // Compile TTL support. Default: 1
```

### 3.5 KV overflow policy

What happens when the KV store is full and a new key is inserted:

```c
// MICRODB_KV_POLICY_OVERWRITE — evict the oldest entry (LRU). Default.
// MICRODB_KV_POLICY_REJECT    — return MICRODB_ERR_FULL, do not insert.
#define MICRODB_KV_OVERFLOW_POLICY  MICRODB_KV_POLICY_OVERWRITE
```

### 3.6 Time-series engine limits

```c
#define MICRODB_TS_MAX_STREAMS    8     // Max number of named streams. Default: 8
#define MICRODB_TS_STREAM_NAME_LEN 16   // Max stream name length incl. NUL. Default: 16
```

### 3.7 Time-series overflow policy

What happens when a stream's ring buffer is full:

```c
// MICRODB_TS_POLICY_DROP_OLDEST  — overwrite oldest sample. Default.
// MICRODB_TS_POLICY_REJECT       — return MICRODB_ERR_FULL, keep old data.
// MICRODB_TS_POLICY_DOWNSAMPLE   — merge two adjacent oldest samples (average).
#define MICRODB_TS_OVERFLOW_POLICY  MICRODB_TS_POLICY_DROP_OLDEST
```

### 3.8 Relational engine limits

```c
#define MICRODB_REL_MAX_TABLES     4    // Max number of tables. Default: 4
#define MICRODB_REL_MAX_COLS      16    // Max columns per table. Default: 16
#define MICRODB_REL_COL_NAME_LEN  16    // Max column name length incl. NUL. Default: 16
#define MICRODB_REL_TABLE_NAME_LEN 16   // Max table name length incl. NUL. Default: 16
```

### 3.9 WAL (power-loss safety)

```c
#define MICRODB_ENABLE_WAL  1           // Enable write-ahead log. Default: 1
                                        // Requires storage HAL.
                                        // If 0: writes go directly to storage (faster, unsafe).
```

### 3.10 Timestamp type

```c
// uint32_t — seconds since epoch or any monotonic counter. Default.
// uint64_t — milliseconds since epoch (requires 64-bit timestamp provider).
#define MICRODB_TIMESTAMP_TYPE  uint32_t
```

---

## 4. Memory model

### 4.1 Single allocation

```
microdb_init()
    │
    └── malloc(MICRODB_RAM_KB * 1024)
            │
            ├── [KV slice]   MICRODB_RAM_KV_PCT  %
            │       ├── hash table      (open addressing, power-of-2 buckets)
            │       └── value store     (packed blob pool)
            │
            ├── [TS slice]   MICRODB_RAM_TS_PCT  %
            │       └── stream ring buffers (one per registered stream)
            │
            └── [REL slice]  MICRODB_RAM_REL_PCT %
                    ├── schema store    (column descriptors)
                    └── row store       (fixed-size rows, packed)
```

All pointers inside `microdb_t` point into this single allocation.
`microdb_deinit()` calls `free()` on the single allocation. Nothing else is ever freed.

### 4.2 What happens if malloc fails

`microdb_init()` returns `MICRODB_ERR_NO_MEM`.
The `microdb_t` handle is left zeroed.
All subsequent calls with that handle return `MICRODB_ERR_INVALID`.

### 4.3 Internal allocator

After the single malloc, microdb uses a trivial bump allocator internally
to hand out slices to each engine:

```c
typedef struct {
    uint8_t *base;
    size_t   used;
    size_t   capacity;
} microdb_arena_t;
```

Sub-allocations happen only during `microdb_init()`.
No sub-allocations happen after init returns.

---

## 5. Storage HAL

microdb does not call any flash/SD/FRAM functions directly.
It calls through a function pointer table you provide at init.

### 5.1 HAL struct

```c
typedef struct {

    /* Read `len` bytes from storage at byte offset `offset` into `buf`.
     * Returns MICRODB_OK or MICRODB_ERR_STORAGE. */
    microdb_err_t (*read)(void *ctx, uint32_t offset, void *buf, size_t len);

    /* Write `len` bytes from `buf` to storage at byte offset `offset`.
     * Storage must already be erased at that location.
     * Returns MICRODB_OK or MICRODB_ERR_STORAGE. */
    microdb_err_t (*write)(void *ctx, uint32_t offset, const void *buf, size_t len);

    /* Erase the sector containing byte offset `offset`.
     * Sector size is implementation-defined (HAL knows it).
     * Returns MICRODB_OK or MICRODB_ERR_STORAGE. */
    microdb_err_t (*erase)(void *ctx, uint32_t offset);

    /* Flush any write caches to physical media (fsync equivalent).
     * Called after every committed transaction.
     * Returns MICRODB_OK or MICRODB_ERR_STORAGE. */
    microdb_err_t (*sync)(void *ctx);

    /* Total usable storage size in bytes.
     * microdb will never access beyond this offset. */
    uint32_t capacity;

    /* Erase block size in bytes.
     * microdb aligns all erase operations to this boundary. */
    uint32_t erase_size;

    /* Write granularity in bytes (minimum write unit).
     * microdb pads writes to a multiple of this value. */
    uint32_t write_size;

    /* Opaque context pointer passed to every HAL function as `ctx`.
     * Can be NULL if not needed. */
    void *ctx;

} microdb_storage_t;
```

### 5.2 HAL contract

- `read` and `write` must be byte-addressable.
- `erase` erases exactly one sector containing the given offset.
- `write` is undefined behavior if called on un-erased storage.
- `sync` must block until data is physically committed.
- HAL functions may be called from any context but must not block indefinitely.
- If storage is NULL in `microdb_cfg_t`, persistence is disabled —
  all data lives in RAM only. WAL is automatically disabled.

### 5.3 Provided HAL implementations

| Port | File | Notes |
|------|------|-------|
| ESP32 NVS/partition | `port/esp32/microdb_port_esp32.c` | Uses `esp_partition_*` API |
| POSIX file | `port/posix/microdb_port_posix.c` | For simulation and tests |
| Volatile RAM | `port/ram/microdb_port_ram.c` | No persistence, testing only |

---

## 6. Error codes

```c
typedef enum {
    MICRODB_OK            =  0,   // Success
    MICRODB_ERR_INVALID   = -1,   // NULL handle, bad argument, or invalid state
    MICRODB_ERR_NO_MEM    = -2,   // malloc failed, or RAM budget exhausted
    MICRODB_ERR_FULL      = -3,   // Store full and policy is REJECT
    MICRODB_ERR_NOT_FOUND = -4,   // Key/record does not exist
    MICRODB_ERR_EXPIRED   = -5,   // Key exists but TTL has elapsed (KV only)
    MICRODB_ERR_STORAGE   = -6,   // HAL read/write/erase/sync returned error
    MICRODB_ERR_CORRUPT   = -7,   // CRC mismatch on stored data
    MICRODB_ERR_SEALED    = -8,   // Schema already sealed, cannot add columns
    MICRODB_ERR_EXISTS    = -9,   // Table or stream with that name already registered
    MICRODB_ERR_DISABLED  = -10,  // Engine compiled out (MICRODB_ENABLE_* = 0)
    MICRODB_ERR_OVERFLOW  = -11,  // Value too large for MICRODB_KV_VAL_MAX_LEN
    MICRODB_ERR_SCHEMA    = -12,  // Schema violation (wrong type, unknown column)
} microdb_err_t;
```

All API functions return `microdb_err_t`.
Functions that return data do so via output pointer arguments.
No function returns a pointer that the caller must free.

---

## 7. Core API — init / deinit

### 7.1 Types

```c
typedef struct microdb_s microdb_t;   // Opaque handle. Caller allocates on stack or static.
                                      // sizeof(microdb_t) is defined in microdb.h.
                                      // Do NOT copy microdb_t — internal pointers would alias.

typedef struct {
    microdb_storage_t *storage;       // Storage HAL. NULL = RAM-only, no persistence.
    uint32_t           ram_kb;        // RAM budget override. 0 = use MICRODB_RAM_KB.
                                      // Non-zero value overrides the compile-time define.
    microdb_timestamp_t (*now)(void); // Timestamp provider. NULL = timestamps always 0.
} microdb_cfg_t;
```

### 7.2 Functions

```c
/* Initialize microdb. Performs the single malloc.
 *
 * db   — caller-allocated handle (stack or static)
 * cfg  — configuration (may be stack-allocated, not stored after init returns)
 *
 * Returns MICRODB_OK on success.
 * Returns MICRODB_ERR_INVALID if db or cfg is NULL.
 * Returns MICRODB_ERR_NO_MEM if malloc fails.
 * Returns MICRODB_ERR_STORAGE if WAL recovery from storage fails. */
microdb_err_t microdb_init(microdb_t *db, const microdb_cfg_t *cfg);


/* Flush all dirty data to storage and free the single allocation.
 * After this call, db is zeroed and must not be used.
 *
 * Returns MICRODB_OK on success.
 * Returns MICRODB_ERR_STORAGE if final flush fails (data may be partially persisted). */
microdb_err_t microdb_deinit(microdb_t *db);


/* Flush all dirty data to storage without deinitializing.
 * Safe to call at any time (e.g. before deep sleep).
 *
 * Returns MICRODB_OK or MICRODB_ERR_STORAGE. */
microdb_err_t microdb_flush(microdb_t *db);


/* Fill in statistics for the current state of the database.
 * All fields are informational — do not use for capacity decisions at runtime. */
typedef struct {
    size_t ram_total_bytes;          // Total RAM allocated
    size_t ram_used_bytes;           // RAM currently used by live data
    uint32_t kv_entries;             // Live KV entries
    uint32_t kv_capacity;            // Max KV entries
    uint32_t ts_streams;             // Registered TS streams
    uint32_t rel_tables;             // Registered REL tables
    uint32_t storage_bytes_written;  // Cumulative bytes written to storage since init
} microdb_stats_t;

microdb_err_t microdb_stats(const microdb_t *db, microdb_stats_t *out);
```

---

## 8. KV engine

### 8.1 Concepts

- Keys are null-terminated C strings, max `MICRODB_KV_KEY_MAX_LEN` bytes including NUL.
- Values are binary blobs, max `MICRODB_KV_VAL_MAX_LEN` bytes.
- TTL is in seconds. `0` means no expiry.
- When storage HAL is provided, every set/del is persisted (via WAL if enabled).
- Hash table uses open addressing with linear probing.
- Load factor target: 75%. Bucket count is the smallest power of 2 that fits
  `MICRODB_KV_MAX_KEYS / 0.75` entries.

### 8.2 KV API

```c
/* Store a key-value pair.
 *
 * key   — null-terminated string, max MICRODB_KV_KEY_MAX_LEN - 1 chars
 * val   — pointer to value bytes (copied into internal store)
 * len   — number of bytes to copy from val, max MICRODB_KV_VAL_MAX_LEN
 * ttl   — time-to-live in seconds. 0 = no expiry. Requires MICRODB_KV_ENABLE_TTL=1.
 *
 * If key already exists: overwrites value and resets TTL.
 * If store is full and policy is OVERWRITE: evicts LRU entry first.
 * If store is full and policy is REJECT: returns MICRODB_ERR_FULL.
 *
 * Returns MICRODB_OK on success.
 * Returns MICRODB_ERR_INVALID if key is NULL, empty, or too long.
 * Returns MICRODB_ERR_OVERFLOW if len > MICRODB_KV_VAL_MAX_LEN.
 * Returns MICRODB_ERR_FULL if policy is REJECT and store is full.
 * Returns MICRODB_ERR_STORAGE if persistence fails. */
microdb_err_t microdb_kv_set(microdb_t *db,
                              const char *key,
                              const void *val,
                              size_t      len,
                              uint32_t    ttl);

/* Shorthand — no TTL (same as ttl=0) */
#define microdb_kv_put(db, key, val, len) \
        microdb_kv_set((db), (key), (val), (len), 0)


/* Retrieve a value by key.
 *
 * buf     — output buffer (caller-allocated)
 * buf_len — size of buf in bytes
 * out_len — set to actual value length on success (may be NULL)
 *
 * Returns MICRODB_OK on success.
 * Returns MICRODB_ERR_NOT_FOUND if key does not exist.
 * Returns MICRODB_ERR_EXPIRED if key exists but TTL has elapsed
 *         (entry is deleted as a side effect).
 * Returns MICRODB_ERR_OVERFLOW if buf_len < actual value length
 *         (buf is not written, out_len is set to required size). */
microdb_err_t microdb_kv_get(microdb_t  *db,
                              const char *key,
                              void       *buf,
                              size_t      buf_len,
                              size_t     *out_len);


/* Delete a key.
 *
 * Returns MICRODB_OK on success.
 * Returns MICRODB_ERR_NOT_FOUND if key does not exist. */
microdb_err_t microdb_kv_del(microdb_t *db, const char *key);


/* Check if key exists without fetching value.
 * Also checks TTL expiry — expired keys return MICRODB_ERR_EXPIRED.
 *
 * Returns MICRODB_OK if key exists and is not expired.
 * Returns MICRODB_ERR_NOT_FOUND if key does not exist.
 * Returns MICRODB_ERR_EXPIRED if key exists but has expired. */
microdb_err_t microdb_kv_exists(microdb_t *db, const char *key);


/* Iterate over all live (non-expired) KV entries.
 * Callback is called once per entry in undefined order.
 * Do not call microdb_kv_set/del from inside the callback — behavior is undefined.
 *
 * cb(key, val, val_len, ttl_remaining, ctx)
 *   ttl_remaining: seconds until expiry, or UINT32_MAX if no TTL.
 *   Return true to continue iteration, false to stop early.
 *
 * Returns MICRODB_OK after full iteration or early stop.
 * Returns MICRODB_ERR_INVALID if cb is NULL. */
typedef bool (*microdb_kv_iter_cb_t)(const char *key,
                                      const void *val,
                                      size_t      val_len,
                                      uint32_t    ttl_remaining,
                                      void       *ctx);

microdb_err_t microdb_kv_iter(microdb_t         *db,
                               microdb_kv_iter_cb_t cb,
                               void              *ctx);


/* Remove all expired entries now (normally done lazily on get).
 * Useful to call before sleep to reclaim space. */
microdb_err_t microdb_kv_purge_expired(microdb_t *db);


/* Remove all KV entries (RAM and storage). */
microdb_err_t microdb_kv_clear(microdb_t *db);
```

---

## 9. Time-series engine

### 9.1 Concepts

- A **stream** is a named sequence of `(timestamp, value)` samples.
- Streams must be **registered** before use via `microdb_ts_register()`.
- Registration must happen before any insert/query.
- Value type per stream is fixed at registration (float, int32, uint32, or raw bytes).
- Each stream gets a slice of the TS RAM budget (equal share by default).
- Ring buffer: oldest sample is overwritten when full (or rejected — per policy).
- Timestamps are monotonically increasing within a stream (not enforced, but assumed).
- Queries are by time range: `[from, to]` inclusive.

### 9.2 Value types

```c
typedef enum {
    MICRODB_TS_F32  = 0,   // float  (4 bytes)
    MICRODB_TS_I32  = 1,   // int32_t (4 bytes)
    MICRODB_TS_U32  = 2,   // uint32_t (4 bytes)
    MICRODB_TS_RAW  = 3,   // raw bytes — size fixed at registration
} microdb_ts_type_t;
```

### 9.3 Sample struct

```c
typedef struct {
    MICRODB_TIMESTAMP_TYPE ts;   // Timestamp
    union {
        float    f32;
        int32_t  i32;
        uint32_t u32;
        uint8_t  raw[MICRODB_TS_RAW_MAX];  // only valid for MICRODB_TS_RAW
    } v;
} microdb_ts_sample_t;

#define MICRODB_TS_RAW_MAX  16   // Max raw bytes per sample. Override with #define.
```

### 9.4 TS API

```c
/* Register a stream. Must be called before any insert/query on that stream.
 * All registrations must happen before the first insert.
 *
 * name     — unique stream name, max MICRODB_TS_STREAM_NAME_LEN - 1 chars
 * type     — value type for this stream
 * raw_size — only used when type == MICRODB_TS_RAW, ignored otherwise
 *
 * Returns MICRODB_OK on success.
 * Returns MICRODB_ERR_EXISTS if stream with that name already registered.
 * Returns MICRODB_ERR_FULL if MICRODB_TS_MAX_STREAMS already registered.
 * Returns MICRODB_ERR_NO_MEM if TS RAM slice is exhausted. */
microdb_err_t microdb_ts_register(microdb_t         *db,
                                   const char        *name,
                                   microdb_ts_type_t  type,
                                   size_t             raw_size);


/* Insert a sample into a stream.
 *
 * name — stream name (must have been registered)
 * ts   — timestamp
 * val  — pointer to value:
 *         float*    for MICRODB_TS_F32
 *         int32_t*  for MICRODB_TS_I32
 *         uint32_t* for MICRODB_TS_U32
 *         void*     for MICRODB_TS_RAW (must be raw_size bytes)
 *
 * Returns MICRODB_OK on success (including when oldest sample was evicted).
 * Returns MICRODB_ERR_NOT_FOUND if stream not registered.
 * Returns MICRODB_ERR_FULL if policy is REJECT and buffer is full.
 * Returns MICRODB_ERR_INVALID if val is NULL. */
microdb_err_t microdb_ts_insert(microdb_t              *db,
                                 const char             *name,
                                 MICRODB_TIMESTAMP_TYPE  ts,
                                 const void             *val);


/* Get the most recent sample from a stream.
 *
 * out — filled with the most recent sample on success.
 *
 * Returns MICRODB_OK on success.
 * Returns MICRODB_ERR_NOT_FOUND if stream has no samples yet. */
microdb_err_t microdb_ts_last(microdb_t         *db,
                               const char        *name,
                               microdb_ts_sample_t *out);


/* Query samples in time range [from, to] inclusive.
 * Calls cb() for each matching sample in chronological order.
 *
 * cb(sample, ctx) — return true to continue, false to stop.
 *
 * Returns MICRODB_OK after iteration.
 * Returns MICRODB_ERR_NOT_FOUND if stream not registered.
 * Returns MICRODB_ERR_INVALID if cb is NULL. */
typedef bool (*microdb_ts_query_cb_t)(const microdb_ts_sample_t *sample, void *ctx);

microdb_err_t microdb_ts_query(microdb_t              *db,
                                const char             *name,
                                MICRODB_TIMESTAMP_TYPE  from,
                                MICRODB_TIMESTAMP_TYPE  to,
                                microdb_ts_query_cb_t   cb,
                                void                   *ctx);


/* Convenience wrapper — collect up to max_count samples into a caller-provided buffer.
 * Fills buf[0..n-1] in chronological order. Sets *out_count to number written.
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_OVERFLOW if more samples matched than max_count
 *         (first max_count are returned, out_count == max_count). */
microdb_err_t microdb_ts_query_buf(microdb_t               *db,
                                    const char              *name,
                                    MICRODB_TIMESTAMP_TYPE   from,
                                    MICRODB_TIMESTAMP_TYPE   to,
                                    microdb_ts_sample_t     *buf,
                                    size_t                   max_count,
                                    size_t                  *out_count);


/* Count samples in [from, to]. Does not invoke callback. O(n). */
microdb_err_t microdb_ts_count(microdb_t              *db,
                                const char             *name,
                                MICRODB_TIMESTAMP_TYPE  from,
                                MICRODB_TIMESTAMP_TYPE  to,
                                size_t                 *out_count);


/* Clear all samples from a stream. Schema (registration) is preserved. */
microdb_err_t microdb_ts_clear(microdb_t *db, const char *name);
```

---

## 10. Relational engine

### 10.1 Concepts

- A **table** has a name, a **schema**, and a set of **rows**.
- Schema is defined at runtime using `microdb_schema_*` functions.
- Once `microdb_schema_seal()` is called, no new columns can be added.
- Rows are fixed-size (computed from schema). No variable-length rows.
- Each table gets a slice of the REL RAM budget.
- Max rows per table is declared at schema creation.
- One index per table on one nominated column (the **index column**).
  Index is a sorted array for O(log n) find. Insert is O(n) to maintain sort.
- No JOINs. No SQL. C API only.

### 10.2 Column types

```c
typedef enum {
    MICRODB_COL_U8   = 0,   // uint8_t    (1 byte)
    MICRODB_COL_U16  = 1,   // uint16_t   (2 bytes)
    MICRODB_COL_U32  = 2,   // uint32_t   (4 bytes)
    MICRODB_COL_U64  = 3,   // uint64_t   (8 bytes)
    MICRODB_COL_I8   = 4,   // int8_t     (1 byte)
    MICRODB_COL_I16  = 5,   // int16_t    (2 bytes)
    MICRODB_COL_I32  = 6,   // int32_t    (4 bytes)
    MICRODB_COL_I64  = 7,   // int64_t    (8 bytes)
    MICRODB_COL_F32  = 8,   // float      (4 bytes)
    MICRODB_COL_F64  = 9,   // double     (8 bytes)
    MICRODB_COL_BOOL = 10,  // bool       (1 byte)
    MICRODB_COL_STR  = 11,  // char[]     (size specified at column definition)
    MICRODB_COL_BLOB = 12,  // uint8_t[]  (size specified at column definition)
} microdb_col_type_t;
```

### 10.3 REL API — schema

```c
/* Initialize a schema descriptor (stack-allocatable, small struct).
 *
 * schema    — caller-allocated schema handle
 * name      — table name, max MICRODB_REL_TABLE_NAME_LEN - 1 chars
 * max_rows  — maximum number of rows this table may hold
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_INVALID if name is NULL/empty or max_rows is 0. */
microdb_err_t microdb_schema_init(microdb_schema_t *schema,
                                   const char       *name,
                                   uint32_t          max_rows);


/* Add a column to the schema.
 *
 * col_name — column name, max MICRODB_REL_COL_NAME_LEN - 1 chars
 * type     — column type
 * size     — byte size of the column:
 *             For scalar types (U8..BOOL): must equal sizeof(type) — enforced.
 *             For STR: max string length including NUL.
 *             For BLOB: byte array length.
 * is_index — true: this column is the index column (max one per table).
 *             Setting is_index=true on a second column returns MICRODB_ERR_INVALID.
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_SEALED if schema is already sealed.
 * Returns MICRODB_ERR_FULL if MICRODB_REL_MAX_COLS already defined.
 * Returns MICRODB_ERR_INVALID on bad arguments. */
microdb_err_t microdb_schema_add(microdb_schema_t  *schema,
                                  const char        *col_name,
                                  microdb_col_type_t type,
                                  size_t             size,
                                  bool               is_index);


/* Seal the schema — no more columns may be added.
 * Computes row size, column offsets, and index array size.
 * Must be called before microdb_table_create().
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_INVALID if schema has 0 columns. */
microdb_err_t microdb_schema_seal(microdb_schema_t *schema);
```

### 10.4 REL API — table lifecycle

```c
/* Register a table in the database using a sealed schema.
 * Allocates row storage and index array from the REL RAM slice.
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_INVALID if schema is not sealed.
 * Returns MICRODB_ERR_EXISTS if table with that name already registered.
 * Returns MICRODB_ERR_FULL if MICRODB_REL_MAX_TABLES already registered.
 * Returns MICRODB_ERR_NO_MEM if REL RAM slice is exhausted. */
microdb_err_t microdb_table_create(microdb_t        *db,
                                    microdb_schema_t *schema);


/* Get a handle to an already-registered table by name.
 * The handle is valid for the lifetime of the db.
 *
 * out_table — set to internal table pointer on success
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_NOT_FOUND if no table with that name. */
microdb_err_t microdb_table_get(microdb_t   *db,
                                 const char  *name,
                                 microdb_table_t **out_table);
```

### 10.5 REL API — row operations

All row operations use a flat byte buffer (`row_buf`) that the caller allocates.
Row size is fixed and known after schema seal: `microdb_schema_row_size(schema)`.

```c
/* Get the row size in bytes for a table (same for all rows). */
size_t microdb_table_row_size(const microdb_table_t *table);


/* Helper — write a value into a row buffer at the named column.
 *
 * row_buf  — caller-allocated buffer of microdb_table_row_size() bytes
 * col_name — target column
 * val      — pointer to value to write (must match column type and size)
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_NOT_FOUND if col_name not in schema.
 * Returns MICRODB_ERR_SCHEMA if val size does not match column definition. */
microdb_err_t microdb_row_set(const microdb_table_t *table,
                               void                  *row_buf,
                               const char            *col_name,
                               const void            *val);


/* Helper — read a value from a row buffer at the named column.
 *
 * out     — output buffer (caller-allocated, must match column size)
 * out_len — set to actual column size on success (may be NULL)
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_NOT_FOUND if col_name not in schema. */
microdb_err_t microdb_row_get(const microdb_table_t *table,
                               const void            *row_buf,
                               const char            *col_name,
                               void                  *out,
                               size_t                *out_len);


/* Insert a row. row_buf must be fully populated by the caller.
 *
 * Returns MICRODB_OK.
 * Returns MICRODB_ERR_FULL if table is at max_rows.
 * Returns MICRODB_ERR_INVALID if row_buf is NULL.
 * Returns MICRODB_ERR_STORAGE if persistence fails. */
microdb_err_t microdb_rel_insert(microdb_t             *db,
                                  microdb_table_t       *table,
                                  const void            *row_buf);


/* Find all rows where the index column equals search_val.
 * Calls cb() for each matching row.
 *
 * cb(row_buf, ctx) — return true to continue, false to stop.
 * row_buf is valid only during the callback — do not store the pointer.
 *
 * Returns MICRODB_OK (even if zero rows matched).
 * Returns MICRODB_ERR_INVALID if no index column defined on this table. */
typedef bool (*microdb_rel_iter_cb_t)(const void *row_buf, void *ctx);

microdb_err_t microdb_rel_find(microdb_t             *db,
                                microdb_table_t       *table,
                                const void            *search_val,
                                microdb_rel_iter_cb_t  cb,
                                void                  *ctx);


/* Find first row where any named column equals search_val.
 * Linear scan — O(n). Use microdb_rel_find for indexed lookups.
 *
 * out_buf — caller-allocated buffer of row_size bytes, filled on first match.
 *
 * Returns MICRODB_OK if at least one match found.
 * Returns MICRODB_ERR_NOT_FOUND if no match. */
microdb_err_t microdb_rel_find_by(microdb_t       *db,
                                   microdb_table_t *table,
                                   const char      *col_name,
                                   const void      *search_val,
                                   void            *out_buf);


/* Delete all rows where the index column equals search_val.
 *
 * out_deleted — set to number of rows deleted (may be NULL)
 *
 * Returns MICRODB_OK (even if zero rows deleted).
 * Returns MICRODB_ERR_STORAGE if persistence fails. */
microdb_err_t microdb_rel_delete(microdb_t       *db,
                                  microdb_table_t *table,
                                  const void      *search_val,
                                  uint32_t        *out_deleted);


/* Iterate over all rows in insertion order.
 *
 * Returns MICRODB_OK. */
microdb_err_t microdb_rel_iter(microdb_t             *db,
                                microdb_table_t       *table,
                                microdb_rel_iter_cb_t  cb,
                                void                  *ctx);


/* Count live rows in table. O(1). */
microdb_err_t microdb_rel_count(const microdb_table_t *table, uint32_t *out_count);


/* Delete all rows. Schema is preserved. */
microdb_err_t microdb_rel_clear(microdb_t *db, microdb_table_t *table);
```

---

## 11. Flush and persistence

### 11.1 Write policy

By default microdb uses **write-through** to WAL:
every `microdb_kv_set`, `microdb_ts_insert`, `microdb_rel_insert`
writes to the WAL immediately if storage HAL is provided.

The WAL is compacted to main storage pages on:
1. Explicit `microdb_flush(db)` call.
2. `microdb_deinit(db)`.
3. When WAL reaches 75% of its reserved storage space.

### 11.2 microdb_flush

Safe to call from application code at any time.
Typical call sites:
- Before entering deep sleep
- After a burst of sensor inserts
- Periodically from a maintenance task

### 11.3 No auto-flush timer

microdb does not start any timer or background task.
Flushing is always explicit and always synchronous.
This is intentional — microdb has no RTOS dependency.

---

## 12. Power-loss safety

### 12.1 WAL layout on storage

```
Storage layout (when MICRODB_ENABLE_WAL=1):
┌──────────────────────────────────────────┐
│  WAL region   (first 2 erase blocks)     │
│    WAL header   (magic, sequence, CRC)   │
│    WAL entries  (op, engine, key, data)  │
├──────────────────────────────────────────┤
│  KV page region                          │
├──────────────────────────────────────────┤
│  TS page region                          │
├──────────────────────────────────────────┤
│  REL page region                         │
└──────────────────────────────────────────┘
```

### 12.2 Recovery on init

`microdb_init()` with storage HAL:
1. Reads WAL header — checks magic + CRC.
2. If WAL is valid and has uncommitted entries → replays them.
3. If WAL CRC fails → WAL is reset, main pages are used as-is.
4. Loads KV/TS/REL data from main pages into RAM.

### 12.3 Guarantees

- A write confirmed by `MICRODB_OK` will survive a power loss after `sync()` returns.
- A write in progress at the moment of power loss will be replayed on next init
  (WAL entry was written) or lost (power lost before WAL entry was written).
- Partial WAL entries (power lost mid-write) are detected by per-entry CRC and discarded.

---

## 13. Thread safety

**microdb is not thread-safe by default.**

All functions assume single-threaded access.
Two threads must not call any microdb function concurrently on the same handle.

For multi-core ESP32 (dual-core S3, classic):
- Wrap all microdb calls with a mutex or critical section at application level.
- microdb does not provide any locking primitives internally.
- This is intentional — microdb has no RTOS dependency.

A future `MICRODB_ENABLE_MUTEX` compile flag may add optional
FreeRTOS mutex integration, but is not part of v1.0.

---

## 14. RAM budget split

### 14.1 How the arena is divided

```
Total = MICRODB_RAM_KB * 1024 bytes

KV arena  = Total * MICRODB_RAM_KV_PCT  / 100
TS arena  = Total * MICRODB_RAM_TS_PCT  / 100
REL arena = Total * MICRODB_RAM_REL_PCT / 100
```

Disabled engines contribute their percentage to a "waste" region
that is allocated but unused. This simplifies the allocator.
The agent may choose to redistribute disabled engine budget — but
only if all three PCT defines are adjusted by the user, never automatically.

### 14.2 RAM usage at 32 KB (default split 40/40/20)

```
KV  arena: 12,288 bytes  → hash table + value store
TS  arena: 12,288 bytes  → ring buffers for 8 streams (~1536 bytes each)
REL arena:  6,144 bytes  → 4 tables, up to ~30 rows each at typical schema
Overhead:   1,536 bytes  → microdb_t internals, arena headers, WAL buffer
─────────────────────────
Total:     32,256 bytes  (≤ 32 KB budget)
```

### 14.3 RAM usage at 64 KB

```
KV  arena: 24,576 bytes  → ~150 entries @ 128-byte values
TS  arena: 24,576 bytes  → ~3072 samples per stream (8 streams)
REL arena: 12,288 bytes  → ~80 rows per table at typical schema
```

---

## 15. Compile-time limits and defaults

| Define | Default | Min | Max | Notes |
|--------|---------|-----|-----|-------|
| `MICRODB_RAM_KB` | 32 | 8 | 4096 | Total RAM budget in KB |
| `MICRODB_RAM_KV_PCT` | 40 | 0 | 100 | Must sum to 100 with TS+REL |
| `MICRODB_RAM_TS_PCT` | 40 | 0 | 100 | |
| `MICRODB_RAM_REL_PCT` | 20 | 0 | 100 | |
| `MICRODB_ENABLE_KV` | 1 | 0 | 1 | |
| `MICRODB_ENABLE_TS` | 1 | 0 | 1 | |
| `MICRODB_ENABLE_REL` | 1 | 0 | 1 | |
| `MICRODB_ENABLE_WAL` | 1 | 0 | 1 | Requires storage HAL |
| `MICRODB_KV_MAX_KEYS` | 64 | 1 | 1024 | |
| `MICRODB_KV_KEY_MAX_LEN` | 32 | 4 | 128 | Includes NUL |
| `MICRODB_KV_VAL_MAX_LEN` | 128 | 1 | 1024 | |
| `MICRODB_KV_ENABLE_TTL` | 1 | 0 | 1 | |
| `MICRODB_KV_OVERFLOW_POLICY` | `OVERWRITE` | — | — | |
| `MICRODB_TS_MAX_STREAMS` | 8 | 1 | 64 | |
| `MICRODB_TS_STREAM_NAME_LEN` | 16 | 4 | 64 | Includes NUL |
| `MICRODB_TS_RAW_MAX` | 16 | 1 | 64 | Max bytes for RAW type |
| `MICRODB_TS_OVERFLOW_POLICY` | `DROP_OLDEST` | — | — | |
| `MICRODB_REL_MAX_TABLES` | 4 | 1 | 32 | |
| `MICRODB_REL_MAX_COLS` | 16 | 1 | 64 | Per table |
| `MICRODB_REL_COL_NAME_LEN` | 16 | 4 | 64 | Includes NUL |
| `MICRODB_REL_TABLE_NAME_LEN` | 16 | 4 | 64 | Includes NUL |
| `MICRODB_TIMESTAMP_TYPE` | `uint32_t` | — | — | Or `uint64_t` |

---

## 16. File layout of microdb.h

```c
/* microdb.h — single public header */

#ifndef MICRODB_H
#define MICRODB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── 1. Defaults (user overrides before #include) ───────── */
/* All #ifndef MICRODB_* defaults here */

/* ── 2. Derived constants ──────────────────────────────── */
/* Compile-time assertions: PCT sum == 100, etc. */

/* ── 3. Forward declarations ────────────────────────────── */
typedef struct microdb_s        microdb_t;
typedef struct microdb_schema_s microdb_schema_t;
typedef struct microdb_table_s  microdb_table_t;

/* ── 4. Enums ────────────────────────────────────────────── */
/* microdb_err_t, microdb_ts_type_t, microdb_col_type_t */

/* ── 5. Structs ──────────────────────────────────────────── */
/* microdb_storage_t, microdb_cfg_t, microdb_stats_t,
   microdb_ts_sample_t */

/* ── 6. Opaque handle sizes ──────────────────────────────── */
/* microdb_t, microdb_schema_t, microdb_table_t defined with
   fixed sizes using uint8_t opaque[] arrays so callers can
   stack-allocate without including internal headers. */

/* ── 7. Core API ─────────────────────────────────────────── */
/* microdb_init, microdb_deinit, microdb_flush, microdb_stats */

/* ── 8. KV API ───────────────────────────────────────────── */
#if MICRODB_ENABLE_KV
/* microdb_kv_set, _get, _del, _exists, _iter,
   _purge_expired, _clear, microdb_kv_put macro */
#endif

/* ── 9. TS API ───────────────────────────────────────────── */
#if MICRODB_ENABLE_TS
/* microdb_ts_register, _insert, _last, _query,
   _query_buf, _count, _clear */
#endif

/* ── 10. REL API ─────────────────────────────────────────── */
#if MICRODB_ENABLE_REL
/* microdb_schema_init, _add, _seal
   microdb_table_create, _get, _row_size
   microdb_row_set, _row_get
   microdb_rel_insert, _find, _find_by, _delete,
   _iter, _count, _clear */
#endif

#endif /* MICRODB_H */
```

---

## 17. Implementation notes per engine

### 17.1 KV — hash table

- Open addressing, linear probing.
- Bucket count = smallest power of 2 ≥ `ceil(MICRODB_KV_MAX_KEYS / 0.75)`.
- Each bucket: `{ uint8_t state; char key[KV_KEY_MAX]; uint32_t val_offset; size_t val_len; uint32_t expires_at; }`
- Value store: packed blob pool starting after the bucket array.
  Values are allocated sequentially; deleted slots are marked free.
  On compaction (triggered when fragmentation > 50%): values are
  compacted in-place, offsets in buckets are updated.
- LRU for OVERWRITE policy: each bucket has a `uint32_t last_access` counter.
  Eviction: scan all buckets, evict bucket with lowest `last_access`.

### 17.2 TS — ring buffer

- Each stream has a dedicated ring buffer within the TS arena slice.
- Stream descriptor: `{ char name[]; microdb_ts_type_t type; size_t raw_size;
  uint32_t head; uint32_t tail; uint32_t count; uint32_t capacity; microdb_ts_sample_t *buf; }`
- `capacity` = TS arena for this stream / sizeof(microdb_ts_sample_t).
- `head` = next write position. `tail` = oldest sample.
- Query: iterate from `tail` to `head` wrapping around, filter by timestamp.
- DOWNSAMPLE policy: when full and new sample arrives, find two adjacent oldest
  samples with closest timestamps, replace with averaged sample, insert new one.

### 17.3 REL — packed rows + sorted index

- Row storage: flat packed array `uint8_t rows[max_rows * row_size]`.
- Live rows tracked via a bitmap `uint8_t alive[ceil(max_rows / 8)]`.
- Index: sorted array `{ index_val_copy; uint32_t row_idx }[]` of up to `max_rows` entries.
  Kept sorted by value after every insert/delete (insertion sort — acceptable for small n).
- `microdb_rel_find`: binary search on index array → O(log n).
- `microdb_rel_find_by` (non-index column): linear scan → O(n).
- Row size is computed at `microdb_schema_seal()` as sum of all column sizes,
  naturally aligned to 4-byte boundary.

### 17.4 WAL structure

```c
typedef struct {
    uint32_t magic;         // 0x4D44424C  ("MDBL")
    uint32_t sequence;      // Monotonically increasing per entry
    uint8_t  engine;        // 0=KV 1=TS 2=REL
    uint8_t  op;            // 0=SET/INSERT 1=DEL/CLEAR
    uint16_t data_len;      // Length of following data
    uint32_t crc;           // CRC32 of (sequence + engine + op + data)
    uint8_t  data[];        // Serialized entry data
} microdb_wal_entry_t;
```

---

## 18. Test requirements

Minimum test coverage before v1.0 release:

### 18.1 KV tests (`test_kv.c`) — target 40+ tests

- Set and get back a value → correct bytes
- Get non-existent key → `MICRODB_ERR_NOT_FOUND`
- Delete existing key → `MICRODB_OK`, then get → `MICRODB_ERR_NOT_FOUND`
- Delete non-existent key → `MICRODB_ERR_NOT_FOUND`
- Overwrite existing key → new value returned
- Key exactly `KV_KEY_MAX_LEN - 1` chars → OK
- Key exactly `KV_KEY_MAX_LEN` chars (with NUL) → `MICRODB_ERR_INVALID`
- Value exactly `KV_VAL_MAX_LEN` bytes → OK
- Value `KV_VAL_MAX_LEN + 1` bytes → `MICRODB_ERR_OVERFLOW`
- Fill to max keys with OVERWRITE policy → oldest evicted on next set
- Fill to max keys with REJECT policy → `MICRODB_ERR_FULL`
- TTL=0 → never expires
- TTL=1 → expires after 1 second (mock clock)
- Get expired key → `MICRODB_ERR_EXPIRED`, entry deleted
- `microdb_kv_exists` on live key → `MICRODB_OK`
- `microdb_kv_exists` on expired key → `MICRODB_ERR_EXPIRED`
- `microdb_kv_iter` visits all live entries
- `microdb_kv_iter` callback returning false stops iteration
- `microdb_kv_purge_expired` removes all expired entries
- `microdb_kv_clear` → count becomes 0
- NULL key → `MICRODB_ERR_INVALID`
- Empty string key → `MICRODB_ERR_INVALID`
- Binary value (all-zero, all-0xFF) → stored and retrieved correctly
- CRC corruption of persisted value → `MICRODB_ERR_CORRUPT` on load

### 18.2 TS tests (`test_ts.c`) — target 35+ tests

- Register stream → OK
- Register same name twice → `MICRODB_ERR_EXISTS`
- Insert into unregistered stream → `MICRODB_ERR_NOT_FOUND`
- Insert and last → correct value and timestamp
- Fill ring buffer → oldest dropped (DROP_OLDEST policy)
- Fill ring buffer with REJECT policy → `MICRODB_ERR_FULL`
- Query empty stream → cb never called
- Query all samples → all returned in order
- Query with `from > to` → no results
- Query with exact timestamp match → included
- `microdb_ts_query_buf` exact fit → `MICRODB_OK`
- `microdb_ts_query_buf` buffer too small → `MICRODB_ERR_OVERFLOW`, first N returned
- `microdb_ts_count` → correct count
- `microdb_ts_clear` → count 0
- F32 type → stored and retrieved as float
- I32 type → stored and retrieved as int32
- RAW type → correct bytes
- Callback returning false stops query early
- All 8 streams populated simultaneously → correct isolation

### 18.3 REL tests (`test_rel.c`) — target 40+ tests

- Schema init → OK
- Add column → OK
- Add column after seal → `MICRODB_ERR_SEALED`
- Two index columns → second returns `MICRODB_ERR_INVALID`
- Seal empty schema → `MICRODB_ERR_INVALID`
- Create table → OK
- Create duplicate table name → `MICRODB_ERR_EXISTS`
- Insert row → OK
- Insert beyond max_rows → `MICRODB_ERR_FULL`
- Find by index → correct row
- Find by index not found → cb never called
- Find by non-index column → correct row
- Find by non-index column not found → `MICRODB_ERR_NOT_FOUND`
- Delete by index → `MICRODB_OK`, subsequent find returns nothing
- `microdb_rel_iter` visits all live rows
- `microdb_rel_count` → correct count after inserts and deletes
- `microdb_row_set` unknown column → `MICRODB_ERR_NOT_FOUND`
- `microdb_row_get` unknown column → `MICRODB_ERR_NOT_FOUND`
- STR column overflow (value too long) → `MICRODB_ERR_SCHEMA`
- All column types (U8..BLOB) set and retrieved correctly
- `microdb_rel_clear` → count 0, schema preserved
- `microdb_table_get` → returns correct table handle
- `microdb_table_get` unknown name → `MICRODB_ERR_NOT_FOUND`

### 18.4 WAL tests (`test_wal.c`) — target 25+ tests

- Write KV entry → WAL entry present
- Power loss simulation (truncate WAL) → replay on init
- CRC corruption in WAL entry → entry discarded, others replayed
- WAL compaction triggered at 75% full → main pages updated
- WAL reset on corrupt header → clean state
- Multiple engines in WAL → all replayed correctly

### 18.5 Integration tests (`test_integration.c`) — target 20+ tests

- All three engines simultaneously
- `microdb_flush` between operations
- `microdb_deinit` + `microdb_init` (reload from storage) → data persisted
- RAM-only mode (NULL storage) → no WAL, data lost on deinit
- All engines at various RAM budgets: 8KB, 32KB, 64KB, 128KB
- MICRODB_ENABLE_KV=0 → KV functions return `MICRODB_ERR_DISABLED`

### 18.6 Limits tests (`test_limits.c`) — target 15+ tests

- PCT values not summing to 100 → compile-time assert fires
- RAM_KB=8 → all engines get minimal slices, still functional
- RAM_KB=4096 → no overflow in size calculations

**Total target: ≥ 175 tests**

---

## 19. Usage examples

### 19.1 Minimal — RAM only, KV only

```c
#define MICRODB_RAM_KB      16
#define MICRODB_ENABLE_TS   0
#define MICRODB_ENABLE_REL  0
#define MICRODB_RAM_KV_PCT  100
#define MICRODB_RAM_TS_PCT  0
#define MICRODB_RAM_REL_PCT 0
#include "microdb.h"

static microdb_t db;

void app_main(void) {
    microdb_cfg_t cfg = {
        .storage = NULL,   // RAM only
        .ram_kb  = 0,      // use MICRODB_RAM_KB
        .now     = NULL,   // no TTL
    };
    microdb_init(&db, &cfg);

    float temp = 23.5f;
    microdb_kv_put(&db, "temperature", &temp, sizeof(temp));

    float out;
    microdb_kv_get(&db, "temperature", &out, sizeof(out), NULL);
    // out == 23.5f
}
```

### 19.2 Full — all engines, flash persistence, TTL

```c
#define MICRODB_RAM_KB  32
#include "microdb.h"

extern microdb_storage_t my_flash;   // your HAL

static microdb_t db;

void app_main(void) {
    microdb_cfg_t cfg = {
        .storage = &my_flash,
        .now     = esp_get_time_seconds,   // your timestamp fn
    };
    microdb_init(&db, &cfg);

    /* KV — config with TTL */
    const char *ssid = "MyNetwork";
    microdb_kv_set(&db, "wifi_ssid", ssid, strlen(ssid)+1, 0);   // no TTL
    microdb_kv_set(&db, "auth_token", token, token_len, 3600);    // 1h TTL

    /* TS — sensor streams */
    microdb_ts_register(&db, "temperature", MICRODB_TS_F32, 0);
    microdb_ts_register(&db, "humidity",    MICRODB_TS_F32, 0);

    float t = 23.5f;
    microdb_ts_insert(&db, "temperature", time_now(), &t);

    /* REL — sensor registry */
    microdb_schema_t schema;
    microdb_schema_init(&schema, "sensors", 32);
    microdb_schema_add(&schema, "id",       MICRODB_COL_U16, 2,  true);
    microdb_schema_add(&schema, "name",     MICRODB_COL_STR, 16, false);
    microdb_schema_add(&schema, "value",    MICRODB_COL_F32, 4,  false);
    microdb_schema_seal(&schema);
    microdb_table_create(&db, &schema);

    microdb_table_t *sensors;
    microdb_table_get(&db, "sensors", &sensors);

    uint8_t row[microdb_table_row_size(sensors)];
    uint16_t id = 1;
    microdb_row_set(sensors, row, "id",    &id);
    microdb_row_set(sensors, row, "name",  "temp_sensor_1");
    microdb_row_set(sensors, row, "value", &t);
    microdb_rel_insert(&db, sensors, row);

    /* Flush before sleep */
    microdb_flush(&db);
}
```

### 19.3 TS query with callback

```c
typedef struct { float sum; uint32_t count; } avg_ctx_t;

static bool on_sample(const microdb_ts_sample_t *s, void *ctx) {
    avg_ctx_t *a = ctx;
    a->sum += s->v.f32;
    a->count++;
    return true;
}

void compute_average(microdb_t *db) {
    avg_ctx_t ctx = {0};
    microdb_ts_query(db, "temperature",
                     time_now() - 3600,   // last hour
                     time_now(),
                     on_sample, &ctx);
    float avg = ctx.count ? ctx.sum / ctx.count : 0.0f;
}
```

---

## 20. What microdb is NOT

microdb is not SQLite. It has no SQL parser, no query planner, no B-tree.

microdb is not a general-purpose database. It is tuned for embedded MCU
use cases: configuration storage, sensor time-series, and small device registries.

microdb is not thread-safe. Locking is the caller's responsibility.

microdb is not a filesystem. It does not manage filenames, directories,
or arbitrary byte streams. Use `nvlog` for append-only logs.

microdb does not compress data. Use `microcodec` (separate library) before
inserting into microdb if compression is needed.

microdb does not encrypt data. Use `microcrypt` (separate library) to encrypt
values before storing if confidentiality is needed.

---

*microdb v1.0 spec — C99 · Zero deps · Single malloc · Three engines*
*Part of the micro-toolkit ecosystem — github.com/Vanderhell*
