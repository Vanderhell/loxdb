// SPDX-License-Identifier: MIT
#ifndef LOX_INTERNAL_H
#define LOX_INTERNAL_H

#include "lox.h"

#define LOX_MAGIC 0x4D444230u
#define LOX_WAL_HEADER_SIZE 32u
#define LOX_PAGE_HEADER_SIZE 32u
#define LOX_SUPERBLOCK_SIZE 32u

#if defined(__GNUC__) || defined(__clang__)
#define LOX_UNUSED_FN __attribute__((unused))
#else
#define LOX_UNUSED_FN
#endif

typedef struct {
    uint8_t *base;
    size_t used;
    size_t capacity;
} lox_arena_t;

static inline bool lox_is_power_of_two_size(size_t value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

static inline bool lox_checked_add_size(size_t a, size_t b, size_t *out) {
    if (out == NULL || SIZE_MAX - a < b) {
        return false;
    }
    *out = a + b;
    return true;
}

static inline bool lox_checked_mul_size(size_t a, size_t b, size_t *out) {
    if (out == NULL) {
        return false;
    }
    if (a != 0u && b > SIZE_MAX / a) {
        return false;
    }
    *out = a * b;
    return true;
}

static inline bool lox_checked_add_u32(uint32_t a, uint32_t b, uint32_t *out) {
    uint32_t sum;

    if (out == NULL) {
        return false;
    }
    sum = a + b;
    if (sum < a) {
        return false;
    }
    *out = sum;
    return true;
}

static inline bool lox_checked_align_up_size(size_t value, size_t align, size_t *out) {
    size_t mask;

    if (out == NULL || !lox_is_power_of_two_size(align)) {
        return false;
    }
    mask = align - 1u;
    if (value > SIZE_MAX - mask) {
        return false;
    }
    *out = (value + mask) & ~mask;
    return true;
}

static inline bool lox_checked_u32_from_size(size_t value, uint32_t *out) {
    if (out == NULL || value > (size_t)UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

typedef struct {
    uint8_t state;
    uint32_t key_hash;
    char key[LOX_KV_KEY_MAX_LEN];
    uint32_t val_offset;
    uint32_t val_len;
    uint32_t expires_at;
    uint32_t last_access;
} lox_kv_bucket_t;

typedef struct {
    lox_kv_bucket_t *buckets;
    uint32_t bucket_count;
    uint32_t entry_count;
    uint32_t collision_count;
    uint32_t eviction_count;
    uint8_t *value_store;
    uint32_t value_capacity;
    uint32_t value_used;
    uint32_t live_value_bytes;
    uint32_t access_clock;
} lox_kv_state_t;

typedef struct {
    char key[LOX_KV_KEY_MAX_LEN];
    void *val_ptr;
    size_t val_len;
    uint32_t expires_at;
    uint8_t op;
    uint8_t val_buf[LOX_KV_VAL_MAX_LEN];
} lox_txn_stage_entry_t;

typedef struct {
    char name[LOX_TS_STREAM_NAME_LEN];
    lox_ts_type_t type;
    size_t raw_size;
    uint8_t log_retain_zones;
    uint8_t log_retain_zone_pct;
    uint32_t sample_stride;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t capacity;
    uint8_t *buf;
    bool registered;
} lox_ts_stream_t;

typedef struct {
    lox_ts_stream_t streams[LOX_TS_MAX_STREAMS];
    uint32_t registered_streams;
    uint32_t mutation_seq;
} lox_ts_state_t;

typedef struct {
    char name[LOX_REL_COL_NAME_LEN];
    lox_col_type_t type;
    size_t size;
    size_t offset;
    bool is_index;
} lox_col_desc_t;

typedef struct {
    uint8_t key_bytes[LOX_REL_INDEX_KEY_MAX];
    uint32_t row_idx;
} lox_index_entry_t;

typedef struct lox_core_s lox_core_t;

struct lox_table_s {
    lox_core_t *owner;
    char name[LOX_REL_TABLE_NAME_LEN];
    uint16_t schema_version;
    lox_col_desc_t cols[LOX_REL_MAX_COLS];
    uint32_t col_count;
    uint32_t max_rows;
    size_t row_size;
    uint32_t index_col;
    size_t index_key_size;
    uint8_t *rows;
    uint8_t *alive_bitmap;
    lox_index_entry_t *index;
    uint32_t *order;
    uint32_t live_count;
    uint32_t index_count;
    uint32_t order_count;
    uint32_t mutation_seq;
    bool registered;
};

typedef struct {
    struct lox_table_s tables[LOX_REL_MAX_TABLES];
    uint32_t registered_tables;
} lox_rel_state_t;

typedef struct {
    uint32_t wal_offset;
    uint32_t wal_size;
    uint32_t super_a_offset;
    uint32_t super_b_offset;
    uint32_t super_size;
    uint32_t bank_a_offset;
    uint32_t bank_b_offset;
    uint32_t bank_size;
    uint32_t kv_size;
    uint32_t ts_size;
    uint32_t rel_size;
    uint32_t total_size;
    uint32_t active_bank;
    uint32_t active_generation;
} lox_storage_layout_t;

struct lox_core_s {
    uint32_t magic;
    uint8_t *heap;
    size_t heap_size;
    size_t live_bytes;
    lox_storage_t *storage;
    lox_timestamp_t (*now)(void);
    void (*lock)(void *hdl);
    void (*unlock)(void *hdl);
    void (*lock_destroy)(void *hdl);
    void *lock_handle;
    uint32_t storage_bytes_written;
    uint32_t compact_count;
    uint32_t reopen_count;
    uint32_t recovery_count;
    lox_err_t last_runtime_error;
    lox_err_t last_recovery_status;
    uint8_t recovery_detail;
    bool wal_enabled;
    lox_arena_t arena;
    lox_arena_t kv_arena;
    lox_arena_t ts_arena;
    lox_arena_t rel_arena;
    lox_kv_state_t kv;
    lox_txn_stage_entry_t *txn_stage;
    uint8_t txn_active;
    uint32_t txn_stage_count;
    uint32_t txn_next_id;
    uint32_t txn_active_id;
    lox_ts_state_t ts;
    lox_rel_state_t rel;
    lox_storage_layout_t layout;
    uint32_t wal_sequence;
    uint32_t wal_entry_count;
    uint32_t wal_used;
    uint8_t wal_compact_auto;
    uint8_t wal_compact_threshold_pct;
    uint8_t wal_sync_mode;
    lox_err_t (*on_migrate)(lox_t *db, const char *table_name, uint16_t old_version, uint16_t new_version);
    bool storage_loading;
    bool wal_replaying;
    uint32_t ts_dropped_samples;
    bool migration_in_progress;
};

typedef struct {
    char name[LOX_REL_TABLE_NAME_LEN];
    uint16_t schema_version;
    lox_col_desc_t cols[LOX_REL_MAX_COLS];
    uint32_t col_count;
    uint32_t max_rows;
    size_t row_size;
    uint32_t index_col;
    bool sealed;
} lox_schema_impl_t;

LOX_STATIC_ASSERT(core_size_fits, sizeof(lox_core_t) <= sizeof(((lox_t *)0)->_opaque));
LOX_STATIC_ASSERT(schema_size_fits, sizeof(lox_schema_impl_t) <= sizeof(((lox_schema_t *)0)->_opaque));
LOX_STATIC_ASSERT(table_size_fits, sizeof(struct lox_table_s) >= (LOX_REL_TABLE_NAME_LEN + sizeof(size_t)));

typedef struct {
    char pad;
    lox_t value;
} lox_handle_alignment_probe_t;

typedef struct {
    char pad;
    lox_schema_t value;
} lox_schema_alignment_probe_t;

LOX_STATIC_ASSERT(handle_align_fits, offsetof(lox_handle_alignment_probe_t, value) % sizeof(long double) == 0);
LOX_STATIC_ASSERT(schema_align_fits, offsetof(lox_schema_alignment_probe_t, value) % sizeof(long double) == 0);

lox_core_t *lox_core(lox_t *db);
const lox_core_t *lox_core_const(const lox_t *db);
lox_err_t lox_kv_init(lox_t *db);
lox_err_t lox_ts_init(lox_t *db);
size_t lox_kv_live_bytes(const lox_t *db);
lox_err_t lox_kv_set_at(lox_t *db, const char *key, const void *val, size_t len, uint32_t expires_at);
lox_err_t lox_storage_bootstrap(lox_t *db);
lox_err_t lox_storage_flush(lox_t *db);
lox_err_t lox_persist_kv_set(lox_t *db, const char *key, const void *val, size_t len, uint32_t expires_at);
lox_err_t lox_persist_kv_del(lox_t *db, const char *key);
lox_err_t lox_persist_kv_clear(lox_t *db);
lox_err_t lox_persist_kv_set_txn(lox_t *db, const char *key, const void *val, size_t len, uint32_t expires_at);
lox_err_t lox_persist_kv_del_txn(lox_t *db, const char *key);
lox_err_t lox_persist_txn_commit(lox_t *db);
lox_err_t lox_persist_ts_insert(lox_t *db, const char *name, lox_timestamp_t ts, const void *val, size_t val_len);
lox_err_t lox_persist_ts_register(lox_t *db,
                                  const char *name,
                                  lox_ts_type_t type,
                                  size_t raw_size,
                                  uint8_t log_retain_zones,
                                  uint8_t log_retain_zone_pct);
lox_err_t lox_persist_ts_clear(lox_t *db, const char *name);
lox_err_t lox_persist_rel_insert(lox_t *db, const lox_table_t *table, const void *row_buf);
lox_err_t lox_persist_rel_delete(lox_t *db, const lox_table_t *table, const void *search_val);
lox_err_t lox_persist_rel_table_create(lox_t *db, const lox_schema_t *schema);
lox_err_t lox_persist_rel_clear(lox_t *db, const lox_table_t *table);

static inline uint32_t lox_wal_header_bytes(const lox_core_t *core);
static inline void lox_record_error(lox_core_t *core, lox_err_t err);

static inline void lox__maybe_compact(lox_t *db) {
    lox_core_t *core = lox_core(db);
    uint32_t wal_total;
    uint32_t wal_used;
    uint32_t wal_fill_pct;
    uint32_t threshold;

    uint32_t header_bytes = lox_wal_header_bytes(core);

    if (!core->wal_enabled || core->layout.wal_size <= header_bytes || core->wal_compact_auto == 0u) {
        return;
    }

    wal_total = core->layout.wal_size - header_bytes;
    wal_used = (core->wal_used > header_bytes) ? (core->wal_used - header_bytes) : 0u;
    wal_fill_pct = (wal_total == 0u) ? 0u : ((wal_used * 100u) / wal_total);
    threshold = (core->wal_compact_threshold_pct != 0u) ? core->wal_compact_threshold_pct : 75u;
    if (wal_fill_pct >= threshold) {
        lox_record_error(core, lox_storage_flush(db));
    }
}

static inline void lox_record_error(lox_core_t *core, lox_err_t err) {
    if (core != NULL && err != LOX_OK) {
        core->last_runtime_error = err;
    }
}

static inline uint32_t lox_wal_header_bytes(const lox_core_t *core) {
    if (core != NULL && core->storage != NULL && core->storage->erase_size != 0u) {
        return core->storage->erase_size;
    }
    return 32u;
}

#endif
