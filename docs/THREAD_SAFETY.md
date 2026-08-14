# Thread Safety Hooks

loxdb exposes four optional lock hooks in `lox_cfg_t`:

- `lock_create`: called exactly once during `lox_init`.
- `lock`: called at entry of selected public API calls when `LOX_THREAD_SAFE=1`.
- `unlock`: called at exit of those API calls.
- `lock_destroy`: called exactly once for every successfully created lock,
  either during failed-init cleanup or during `lox_deinit`.

If `lock_create` returns `NULL`, initialization fails cleanly without calling
`lock_destroy`. If hooks are `NULL`, locking is a no-op.

## API Classification

With `LOX_THREAD_SAFE=1`, operations that read or mutate database state acquire
the configured database lock. This includes KV/TS/REL operations, transaction
operations, flush/compact, table lookup, statistics, pressure/capacity,
admission checks, and self-check.

The following functions do not use a database lock because they operate only
on caller-owned state or immutable schema metadata:

- `lox_preflight` reads caller configuration and storage geometry. The caller
  must not mutate either object during the call.
- `lox_schema_init`, `lox_schema_add`, and `lox_schema_seal` operate on a
  caller-owned schema. Concurrent access to the same schema is unsupported and
  must be serialized by the caller.
- `lox_table_row_size`, `lox_row_set`, and `lox_row_get` read immutable metadata
  in a registered table and operate on caller-owned row buffers. Different row
  buffers may be used concurrently. Concurrent access to the same row buffer
  must be synchronized by the caller.

`lox_table_get` returns a stable pointer into the database handle, not an owned
copy. The pointer remains valid until `lox_deinit`; callers must not retain or
use it after deinitialization. Table schema metadata is immutable after table
creation, while row counts and stored rows remain protected by the owner lock.
Use `lox_rel_count` rather than reading table fields directly.

## Lifecycle Exclusion

`lox_init` and `lox_deinit` are lifecycle boundaries and must not overlap any
other operation on the same `lox_t`, including each other. The thread-safety
hooks serialize ordinary operations; they cannot make access safe after lock
destruction or after the handle storage itself has been cleared. A `lox_t`
object must also not be copied or moved while initialized.

## Callback And Copying Notes

- `lox_kv_iter`, `lox_ts_query`, `lox_rel_find`, and `lox_rel_iter` invoke callbacks without DB lock held, then re-lock before continuing.
- `lox_ts_query`, `lox_rel_find`, and `lox_rel_iter` detect relevant mutation after re-lock and return `LOX_ERR_MODIFIED`.
- `lox_kv_iter` is intentionally weakly consistent: mutations from a callback
  or another thread are safe, but keys added or removed during the walk may or
  may not be observed.
- `lox_rel_find_by` copies row bytes into caller `out_buf` while lock is still held. For larger row sizes this can increase lock hold time and create latency spikes for contending threads.

If a persistent mutation suffers an indeterminate storage failure, the handle
is marked storage-faulted while protected by the same lock. Concurrent and
later mutation, flush, and compact calls observe `LOX_ERR_INDETERMINATE`.
Deinitialize and reopen with a fresh handle before further mutations.

## FreeRTOS Example

```c
#include "lox.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static void *db_lock_create(void) {
    return (void *)xSemaphoreCreateMutex();
}

static void db_lock(void *hdl) {
    xSemaphoreTake((SemaphoreHandle_t)hdl, portMAX_DELAY);
}

static void db_unlock(void *hdl) {
    xSemaphoreGive((SemaphoreHandle_t)hdl);
}

static void db_lock_destroy(void *hdl) {
    vSemaphoreDelete((SemaphoreHandle_t)hdl);
}

lox_cfg_t cfg = {
    .storage = NULL,
    .ram_kb = 32u,
    .lock_create = db_lock_create,
    .lock = db_lock,
    .unlock = db_unlock,
    .lock_destroy = db_lock_destroy
};
```

## Bare-Metal No-Op Example

```c
#include "lox.h"

lox_cfg_t cfg = {
    .storage = NULL,
    .ram_kb = 32u,
    .lock_create = NULL,
    .lock = NULL,
    .unlock = NULL,
    .lock_destroy = NULL
};
```
