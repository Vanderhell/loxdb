// SPDX-License-Identifier: MIT
#include "lox_internal.h"
#include "lox_lock.h"

#include "lox_arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOX_STATIC_ASSERT(core_ram_pct_sum, (LOX_RAM_KV_PCT + LOX_RAM_TS_PCT + LOX_RAM_REL_PCT) == 100u);
LOX_STATIC_ASSERT(core_ram_kb_min, LOX_RAM_KB >= 8u);
LOX_STATIC_ASSERT(core_kv_max_keys_min, LOX_KV_MAX_KEYS >= 1u);
LOX_STATIC_ASSERT(core_kv_key_max_len_min, LOX_KV_KEY_MAX_LEN >= 4u);
LOX_STATIC_ASSERT(core_kv_val_max_len_min, LOX_KV_VAL_MAX_LEN >= 1u);
LOX_STATIC_ASSERT(core_ts_max_streams_min, LOX_TS_MAX_STREAMS >= 1u);
LOX_STATIC_ASSERT(core_rel_max_tables_min, LOX_REL_MAX_TABLES >= 1u);
LOX_STATIC_ASSERT(core_rel_max_cols_min, LOX_REL_MAX_COLS >= 1u);

static bool lox_bytes_from_kb(uint32_t ram_kb, size_t *out) {
    return lox_checked_mul_size((size_t)ram_kb, 1024u, out);
}

static bool lox_slice_bytes(size_t total, uint32_t pct, size_t *out) {
    size_t scaled;

    if (!lox_checked_mul_size(total, (size_t)pct, &scaled)) {
        return false;
    }
    if (out == NULL) {
        return false;
    }
    *out = scaled / 100u;
    return true;
}

static bool lox_engine_enabled(uint32_t engine) {
    if (engine == 0u) {
        return LOX_ENABLE_KV != 0;
    }
    if (engine == 1u) {
        return LOX_ENABLE_TS != 0;
    }
    return LOX_ENABLE_REL != 0;
}

static bool lox_lock_hooks_valid(const lox_cfg_t *cfg) {
    bool any;
    bool all;

    if (cfg == NULL) {
        return false;
    }
    any = cfg->lock_create != NULL || cfg->lock != NULL || cfg->unlock != NULL || cfg->lock_destroy != NULL;
    all = cfg->lock_create != NULL && cfg->lock != NULL && cfg->unlock != NULL && cfg->lock_destroy != NULL;
    return !any || all;
}

static lox_err_t lox_validate_cfg(const lox_cfg_t *cfg) {
    if (cfg == NULL) {
        return LOX_ERR_INVALID;
    }
    if (cfg->wal_compact_auto != 0u &&
        (cfg->wal_compact_threshold_pct == 0u || cfg->wal_compact_threshold_pct > 100u)) {
        return LOX_ERR_INVALID;
    }
    if (cfg->wal_sync_mode > LOX_WAL_SYNC_FLUSH_ONLY || !lox_lock_hooks_valid(cfg)) {
        return LOX_ERR_INVALID;
    }
    if (cfg->storage != NULL &&
        (cfg->storage->read == NULL || cfg->storage->write == NULL ||
         cfg->storage->erase == NULL || cfg->storage->sync == NULL ||
         cfg->storage->erase_size == 0u || cfg->storage->write_size != 1u)) {
        return LOX_ERR_INVALID;
    }
    return LOX_OK;
}

lox_err_t lox_compute_ram_layout(const lox_cfg_t *cfg, lox_ram_layout_t *out) {
    uint32_t weights[3] = {LOX_RAM_KV_PCT, LOX_RAM_TS_PCT, LOX_RAM_REL_PCT};
    uint32_t configured[3];
    uint32_t effective[3] = {0u, 0u, 0u};
    uint32_t weight_sum = 0u;
    uint32_t pct_sum = 0u;
    uint32_t first_enabled = 3u;
    uint32_t last_enabled = 3u;
    uint32_t i;
    bool custom;
    size_t cursor = 0u;
    size_t sizes[3] = {0u, 0u, 0u};
    size_t offsets[3] = {0u, 0u, 0u};
    const size_t alignments[3] = {sizeof(void *), sizeof(uint32_t), sizeof(void *)};

    if (cfg == NULL || out == NULL) {
        return LOX_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));
    configured[0] = cfg->kv_pct;
    configured[1] = cfg->ts_pct;
    configured[2] = cfg->rel_pct;
    custom = configured[0] != 0u || configured[1] != 0u || configured[2] != 0u;

    for (i = 0u; i < 3u; ++i) {
        if (lox_engine_enabled(i)) {
            if (first_enabled == 3u) {
                first_enabled = i;
            }
            last_enabled = i;
            if (custom) {
                if (configured[i] == 0u) {
                    return LOX_ERR_INVALID;
                }
                effective[i] = configured[i];
                pct_sum += configured[i];
            } else {
                weight_sum += weights[i];
            }
        } else if (custom && configured[i] != 0u) {
            return LOX_ERR_INVALID;
        }
    }
    if (first_enabled == 3u) {
        return LOX_ERR_INVALID;
    }
    if (custom) {
        if (pct_sum != 100u) {
            return LOX_ERR_INVALID;
        }
    } else {
        uint32_t assigned = 0u;
        if (weight_sum == 0u) {
            return LOX_ERR_INVALID;
        }
        for (i = 0u; i < 3u; ++i) {
            if (lox_engine_enabled(i)) {
                effective[i] = (weights[i] * 100u) / weight_sum;
                assigned += effective[i];
            }
        }
        effective[first_enabled] += 100u - assigned;
    }

    out->ram_kb = cfg->ram_kb != 0u ? cfg->ram_kb : LOX_RAM_KB;
    if (!lox_bytes_from_kb(out->ram_kb, &out->total_size) || out->total_size > UINT32_MAX) {
        return LOX_ERR_OVERFLOW;
    }
    for (i = 0u; i < 3u; ++i) {
        size_t aligned;
        size_t requested;
        if (!lox_engine_enabled(i)) {
            continue;
        }
        if (!lox_checked_align_up_size(cursor, alignments[i], &aligned) || aligned > out->total_size) {
            return LOX_ERR_NO_MEM;
        }
        offsets[i] = aligned;
        if (i == last_enabled) {
            sizes[i] = out->total_size - aligned;
            cursor = out->total_size;
            continue;
        }
        if (!lox_slice_bytes(out->total_size, effective[i], &requested) ||
            requested > out->total_size - aligned ||
            !lox_checked_add_size(aligned, requested, &cursor)) {
            return LOX_ERR_NO_MEM;
        }
        sizes[i] = requested;
    }

    out->kv_pct = (uint8_t)effective[0];
    out->ts_pct = (uint8_t)effective[1];
    out->rel_pct = (uint8_t)effective[2];
    out->kv_offset = offsets[0];
    out->kv_size = sizes[0];
    out->ts_offset = offsets[1];
    out->ts_size = sizes[1];
    out->rel_offset = offsets[2];
    out->rel_size = sizes[2];
    return LOX_OK;
}

static LOX_UNUSED_FN uint32_t lox_popcount8(uint8_t v) {
    uint32_t c = 0u;
    while (v != 0u) {
        c += (uint32_t)(v & 1u);
        v >>= 1u;
    }
    return c;
}

static void lox_selfcheck_set_first(lox_selfcheck_result_t *out, const char *msg) {
    if (out != NULL && out->first_anomaly[0] == '\0' && msg != NULL) {
        (void)snprintf(out->first_anomaly, sizeof(out->first_anomaly), "%s", msg);
    }
}

#if 0
/* Historical helper kept here for reference while the checked-arithmetic
 * path is driven through the active storage and WAL code. */
static bool lox_align_u32_local(uint32_t value, uint32_t align, uint32_t *out) {
    size_t aligned = 0u;

    if (!lox_checked_align_up_size((size_t)value, (size_t)align, &aligned)) {
        return false;
    }
    return lox_checked_u32_from_size(aligned, out);
}
#endif

static uint32_t lox_kv_snapshot_payload_max_local(uint32_t expiration_size) {
    size_t max_entries;
    size_t per_entry;
    size_t total = 0u;
    size_t max_key_len = (LOX_KV_KEY_MAX_LEN > 0u) ? (size_t)(LOX_KV_KEY_MAX_LEN - 1u) : 0u;
    size_t key_bytes;
    size_t val_bytes;
    size_t tmp = 0u;

    if (!lox_checked_add_size(1u, max_key_len, &key_bytes) ||
        !lox_checked_add_size(key_bytes, 4u, &tmp) ||
        !lox_checked_add_size(tmp, (size_t)LOX_KV_VAL_MAX_LEN, &val_bytes) ||
        !lox_checked_add_size(val_bytes, (size_t)expiration_size, &per_entry)) {
        return 0u;
    }
    max_entries = (LOX_KV_MAX_KEYS > LOX_TXN_STAGE_KEYS) ? (size_t)(LOX_KV_MAX_KEYS - LOX_TXN_STAGE_KEYS) : 0u;
    if (!lox_checked_mul_size(max_entries, per_entry, &total)) {
        return 0u;
    }
    if (total > (size_t)UINT32_MAX) {
        return 0u;
    }
    return (uint32_t)total;
}

lox_err_t lox_compute_storage_layout(const lox_storage_t *storage,
                                     size_t ts_arena_size,
                                     size_t rel_arena_size,
                                     uint32_t expiration_size,
                                     lox_storage_layout_t *out,
                                     uint32_t *out_required_size) {
    uint32_t fixed_size;
    uint32_t banks_size;
    uint32_t need_without_wal;
    uint32_t wal_target;
    uint32_t wal_min;
    uint32_t max_wal;
    size_t value;
    uint32_t payload_max;

    if (storage == NULL || out == NULL || out_required_size == NULL || storage->erase_size == 0u) {
        return LOX_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));
    *out_required_size = 0u;
    out->super_size = storage->erase_size;

    payload_max = lox_kv_snapshot_payload_max_local(expiration_size);
    if (payload_max == 0u ||
        !lox_checked_add_size((size_t)payload_max, LOX_PAGE_HEADER_SIZE, &value) ||
        !lox_checked_align_up_size(value, storage->erase_size, &value) ||
        !lox_checked_u32_from_size(value, &out->kv_size)) {
        return LOX_ERR_OVERFLOW;
    }
#if LOX_ENABLE_TS
    if (!lox_checked_add_size(ts_arena_size, LOX_PAGE_HEADER_SIZE, &value) ||
        !lox_checked_align_up_size(value, storage->erase_size, &value) ||
        !lox_checked_u32_from_size(value, &out->ts_size)) {
        return LOX_ERR_OVERFLOW;
    }
#else
    (void)ts_arena_size;
#endif
#if LOX_ENABLE_REL
    if (!lox_checked_add_size(rel_arena_size, LOX_PAGE_HEADER_SIZE, &value) ||
        !lox_checked_align_up_size(value, storage->erase_size, &value) ||
        !lox_checked_u32_from_size(value, &out->rel_size)) {
        return LOX_ERR_OVERFLOW;
    }
#else
    (void)rel_arena_size;
#endif

    if (!lox_checked_add_u32(out->kv_size, out->ts_size, &out->bank_size) ||
        !lox_checked_add_u32(out->bank_size, out->rel_size, &out->bank_size) ||
        !lox_checked_mul_u32(out->super_size, 2u, &fixed_size) ||
        !lox_checked_mul_u32(out->bank_size, 2u, &banks_size) ||
        !lox_checked_add_u32(fixed_size, banks_size, &need_without_wal) ||
        !lox_checked_mul_u32(storage->erase_size, 8u, &wal_target) ||
        !lox_checked_mul_u32(storage->erase_size, 2u, &wal_min) ||
        !lox_checked_add_u32(need_without_wal, wal_min, out_required_size)) {
        return LOX_ERR_OVERFLOW;
    }

    out->wal_size = wal_min;
    if (storage->capacity >= *out_required_size) {
        max_wal = storage->capacity - need_without_wal;
        out->wal_size = (max_wal / storage->erase_size) * storage->erase_size;
        if (out->wal_size > wal_target) {
            out->wal_size = wal_target;
        }
        if (out->wal_size < wal_min) {
            out->wal_size = wal_min;
        }
    }
    if (!lox_checked_add_u32(out->wal_offset, out->wal_size, &out->super_a_offset) ||
        !lox_checked_add_u32(out->super_a_offset, out->super_size, &out->super_b_offset) ||
        !lox_checked_add_u32(out->super_b_offset, out->super_size, &out->bank_a_offset) ||
        !lox_checked_add_u32(out->bank_a_offset, out->bank_size, &out->bank_b_offset) ||
        !lox_checked_add_u32(out->bank_b_offset, out->bank_size, &out->total_size)) {
        return LOX_ERR_OVERFLOW;
    }
    return storage->capacity >= *out_required_size ? LOX_OK : LOX_ERR_STORAGE;
}

const char *lox_err_to_string(lox_err_t err) {
    switch (err) {
        case LOX_OK:
            return "LOX_OK";
        case LOX_ERR_INVALID:
            return "LOX_ERR_INVALID";
        case LOX_ERR_NO_MEM:
            return "LOX_ERR_NO_MEM";
        case LOX_ERR_FULL:
            return "LOX_ERR_FULL";
        case LOX_ERR_NOT_FOUND:
            return "LOX_ERR_NOT_FOUND";
        case LOX_ERR_EXPIRED:
            return "LOX_ERR_EXPIRED";
        case LOX_ERR_STORAGE:
            return "LOX_ERR_STORAGE";
        case LOX_ERR_CORRUPT:
            return "LOX_ERR_CORRUPT";
        case LOX_ERR_SEALED:
            return "LOX_ERR_SEALED";
        case LOX_ERR_EXISTS:
            return "LOX_ERR_EXISTS";
        case LOX_ERR_DISABLED:
            return "LOX_ERR_DISABLED";
        case LOX_ERR_OVERFLOW:
            return "LOX_ERR_OVERFLOW";
        case LOX_ERR_SCHEMA:
            return "LOX_ERR_SCHEMA";
        case LOX_ERR_TXN_ACTIVE:
            return "LOX_ERR_TXN_ACTIVE";
        case LOX_ERR_MODIFIED:
            return "LOX_ERR_MODIFIED";
        case LOX_ERR_INDETERMINATE:
            return "LOX_ERR_INDETERMINATE";
        default:
            return "LOX_ERR_UNKNOWN";
    }
}

uint32_t lox_config_fingerprint(void) {
    return 0x6C6F7864u ^ (uint32_t)LOX_HANDLE_SIZE ^ ((uint32_t)LOX_SCHEMA_SIZE << 1u) ^
           ((uint32_t)sizeof(LOX_TIMESTAMP_TYPE) << 2u) ^ ((uint32_t)LOX_TS_RAW_MAX << 3u) ^
           ((uint32_t)LOX_REL_INDEX_KEY_MAX << 4u) ^ 0x20u ^ ((uint32_t)LOX_PROFILE_CORE_MIN << 6u) ^
           ((uint32_t)LOX_PROFILE_CORE_WAL << 7u) ^ ((uint32_t)LOX_PROFILE_CORE_PERF << 8u) ^
           ((uint32_t)LOX_PROFILE_CORE_HIMEM << 9u) ^ ((uint32_t)LOX_PROFILE_FOOTPRINT_MIN << 10u) ^
           ((uint32_t)LOX_ENABLE_KV << 11u) ^ ((uint32_t)LOX_ENABLE_TS << 12u) ^
           ((uint32_t)LOX_ENABLE_REL << 13u) ^ ((uint32_t)LOX_ENABLE_WAL << 14u) ^
           ((uint32_t)LOX_THREAD_SAFE << 15u) ^ ((uint32_t)LOX_KV_MAX_KEYS << 16u) ^
           ((uint32_t)LOX_KV_KEY_MAX_LEN << 17u) ^ ((uint32_t)LOX_KV_VAL_MAX_LEN << 18u) ^
           ((uint32_t)LOX_TXN_STAGE_KEYS << 19u) ^ ((uint32_t)LOX_TS_MAX_STREAMS << 20u) ^
           ((uint32_t)LOX_TS_STREAM_NAME_LEN << 21u) ^ ((uint32_t)LOX_REL_MAX_TABLES << 22u) ^
           ((uint32_t)LOX_REL_MAX_COLS << 23u) ^ ((uint32_t)LOX_REL_COL_NAME_LEN << 24u) ^
           ((uint32_t)LOX_REL_TABLE_NAME_LEN << 25u);
}

lox_err_t lox_preflight(const lox_cfg_t *cfg, lox_preflight_report_t *out) {
    lox_ram_layout_t ram;
    lox_storage_layout_t storage_layout;
    lox_err_t err;

    if (out == NULL) {
        return LOX_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));
    out->status = LOX_ERR_INVALID;
    err = lox_validate_cfg(cfg);
    if (err != LOX_OK) {
        return err;
    }

    err = lox_compute_ram_layout(cfg, &ram);
    if (err != LOX_OK) {
        out->status = err;
        return err;
    }
    out->ram_kb = ram.ram_kb;
    out->kv_pct = ram.kv_pct;
    out->ts_pct = ram.ts_pct;
    out->rel_pct = ram.rel_pct;
    out->heap_total_bytes = (uint32_t)ram.total_size;
    out->kv_arena_bytes = (uint32_t)ram.kv_size;
    out->ts_arena_bytes = (uint32_t)ram.ts_size;
    out->rel_arena_bytes = (uint32_t)ram.rel_size;
    out->wal_enabled = (cfg->storage != NULL) && (LOX_ENABLE_WAL != 0);

    if (cfg->storage != NULL) {
        out->storage_capacity_bytes = cfg->storage->capacity;
        out->storage_erase_size = cfg->storage->erase_size;
        out->storage_write_size = cfg->storage->write_size;
        err = lox_compute_storage_layout(cfg->storage,
                                         ram.ts_size,
                                         ram.rel_size,
                                         8u,
                                         &storage_layout,
                                         &out->storage_required_bytes);
        out->wal_size = storage_layout.wal_size;
        out->wal_offset = storage_layout.wal_offset;
        out->super_a_offset = storage_layout.super_a_offset;
        out->super_b_offset = storage_layout.super_b_offset;
        out->superblock_bytes = storage_layout.super_size;
        out->bank_a_offset = storage_layout.bank_a_offset;
        out->bank_b_offset = storage_layout.bank_b_offset;
        out->bank_size = storage_layout.bank_size;
        out->kv_snapshot_bytes = storage_layout.kv_size;
        out->ts_snapshot_bytes = storage_layout.ts_size;
        out->rel_snapshot_bytes = storage_layout.rel_size;
        out->storage_layout_bytes = storage_layout.total_size;
        if (err != LOX_OK) {
            out->status = err;
            return err;
        }
    }

    out->status = LOX_OK;
    return LOX_OK;
}

lox_core_t *lox_core(lox_t *db) {
    return (lox_core_t *)&db->_opaque[0];
}

const lox_core_t *lox_core_const(const lox_t *db) {
    return (const lox_core_t *)&db->_opaque[0];
}

static lox_err_t lox_validate_handle(const lox_t *db) {
    if (db == NULL) {
        return LOX_ERR_INVALID;
    }

    if (lox_core_const(db)->magic != LOX_MAGIC) {
        return LOX_ERR_INVALID;
    }

    return LOX_OK;
}

static uint8_t lox_fill_pct_u32(uint32_t used, uint32_t total) {
    if (total == 0u) {
        return 0u;
    }
    return (uint8_t)(((uint64_t)used * 100u) / (uint64_t)total);
}

static uint32_t lox_kv_tombstone_count(const lox_core_t *core) {
    uint32_t i;
    uint32_t tombstones = 0u;
    for (i = 0u; i < core->kv.bucket_count; ++i) {
        if (core->kv.buckets[i].state == 2u) {
            tombstones++;
        }
    }
    return tombstones;
}

static LOX_UNUSED_FN uint32_t lox_kv_live_value_bytes_local(const lox_core_t *core) {
    return core->kv.live_value_bytes;
}

/* Kept as reference helpers; the active code paths use the direct core accessors
 * and checked-arithmetic helpers instead. */
static LOX_UNUSED_FN const lox_kv_bucket_t *lox_kv_find_bucket_const(const lox_core_t *core, const char *key) {
    uint32_t i;
    for (i = 0u; i < core->kv.bucket_count; ++i) {
        const lox_kv_bucket_t *bucket = &core->kv.buckets[i];
        if (bucket->state == 1u && strncmp(bucket->key, key, LOX_KV_KEY_MAX_LEN) == 0) {
            return bucket;
        }
    }
    return NULL;
}

static LOX_UNUSED_FN const lox_ts_stream_t *lox_ts_find_const(const lox_core_t *core, const char *name) {
    uint32_t i;
    for (i = 0u; i < LOX_TS_MAX_STREAMS; ++i) {
        const lox_ts_stream_t *stream = &core->ts.streams[i];
        if (stream->registered && strcmp(stream->name, name) == 0) {
            return stream;
        }
    }
    return NULL;
}
static LOX_UNUSED_FN const lox_table_t *lox_rel_find_table_const(const lox_core_t *core, const char *name) {
    uint32_t i;
    for (i = 0u; i < LOX_REL_MAX_TABLES; ++i) {
        const lox_table_t *table = &core->rel.tables[i];
        if (table->registered && strcmp(table->name, name) == 0) {
            return table;
        }
    }
    return NULL;
}

static uint32_t lox_wal_entry_size_for_payload(uint32_t payload_len) {
    size_t aligned = 0u;

    if (!lox_checked_align_up_size((size_t)payload_len, 4u, &aligned) ||
        !lox_checked_add_size(16u, aligned, &aligned) ||
        !lox_checked_u32_from_size(aligned, &payload_len)) {
        return 0u;
    }
    return payload_len;
}

static void lox_fill_wal_admission(const lox_core_t *core,
                                       uint32_t required_wal_bytes,
                                       uint8_t *out_would_compact,
                                       uint32_t *out_wal_free) {
    uint32_t wal_free = 0u;
    uint8_t would_compact = 0u;
    uint32_t header_bytes = lox_wal_header_bytes(core);

    if (core->wal_enabled && core->layout.wal_size > header_bytes) {
        wal_free = (core->wal_used < core->layout.wal_size) ? (core->layout.wal_size - core->wal_used) : 0u;
        if (required_wal_bytes > wal_free) {
            would_compact = 1u;
        } else if (core->wal_compact_auto != 0u) {
            uint32_t threshold = (core->wal_compact_threshold_pct != 0u) ? core->wal_compact_threshold_pct : 75u;
            uint32_t total = core->layout.wal_size - header_bytes;
            size_t used_after = (size_t)((core->wal_used > header_bytes) ? (core->wal_used - header_bytes) : 0u);
            uint32_t fill = 0u;
            if (!lox_checked_add_size(used_after, (size_t)required_wal_bytes, &used_after)) {
                would_compact = 1u;
                used_after = 0u;
            }
            if (used_after <= (size_t)UINT32_MAX) {
                fill = lox_fill_pct_u32((uint32_t)used_after, total);
            }
            if (used_after > (size_t)UINT32_MAX || fill >= threshold) {
                would_compact = 1u;
            }
        }
    }
    *out_would_compact = would_compact;
    *out_wal_free = wal_free;
}

lox_err_t lox_init(lox_t *db, const lox_cfg_t *cfg) {
    lox_core_t *core = NULL;
    lox_ram_layout_t ram;
    lox_err_t err;
    bool lock_created = false;

    if (db == NULL || cfg == NULL) {
        return LOX_ERR_INVALID;
    }

    memset(db, 0, sizeof(*db));
    core = lox_core(db);

    err = lox_validate_cfg(cfg);
    if (err != LOX_OK) {
        return err;
    }
    err = lox_compute_ram_layout(cfg, &ram);
    if (err != LOX_OK) {
        return err;
    }

    core->heap = (uint8_t *)malloc(ram.total_size);
    if (core->heap == NULL) {
        LOX_LOG("ERROR",
                    "malloc(%u) failed for RAM budget",
                    (unsigned)ram.total_size);
        err = LOX_ERR_NO_MEM;
        goto cleanup;
    }

    memset(core->heap, 0, ram.total_size);
    core->magic = LOX_MAGIC;
    core->heap_size = ram.total_size;
    core->storage = cfg->storage;
    core->now = cfg->now;
    core->lock = cfg->lock;
    core->unlock = cfg->unlock;
    core->lock_destroy = cfg->lock_destroy;
    if (cfg->lock_create != NULL) {
        core->lock_handle = cfg->lock_create();
        if (core->lock_handle == NULL) {
            err = LOX_ERR_NO_MEM;
            goto cleanup;
        }
        lock_created = true;
    }
    core->wal_compact_auto = cfg->wal_compact_auto;
    core->wal_compact_threshold_pct = cfg->wal_compact_threshold_pct;
    core->wal_sync_mode = cfg->wal_sync_mode;
    core->on_migrate = cfg->on_migrate;
    core->last_runtime_error = LOX_OK;
    core->last_recovery_status = LOX_OK;
    core->recovery_detail = LOX_RECOVERY_DETAIL_CLEAN;
    core->wal_enabled = (cfg->storage != NULL) && (LOX_ENABLE_WAL != 0);
    lox_arena_init(&core->arena, core->heap, ram.total_size);
    lox_arena_init(&core->kv_arena, core->heap + ram.kv_offset, ram.kv_size);
    lox_arena_init(&core->ts_arena, core->heap + ram.ts_offset, ram.ts_size);
    lox_arena_init(&core->rel_arena, core->heap + ram.rel_offset, ram.rel_size);

#if LOX_ENABLE_KV
    err = lox_kv_init(db);
    if (err != LOX_OK) {
        goto cleanup;
    }
#endif

#if LOX_ENABLE_TS
    err = lox_ts_init(db);
    if (err != LOX_OK) {
        goto cleanup;
    }
#endif

    err = lox_storage_bootstrap(db);
    if (err != LOX_OK) {
        core->last_runtime_error = err;
        if (err == LOX_ERR_STORAGE && cfg->storage != NULL) {
            LOX_LOG("ERROR",
                        "Storage capacity %u too small, need %u bytes",
                        (unsigned)cfg->storage->capacity,
                        (unsigned)core->layout.total_size);
        }
        goto cleanup;
    }

#if LOX_ENABLE_KV
    core->live_bytes = lox_kv_live_bytes(db);
#endif
    core->runtime_ready = true;
    return LOX_OK;

cleanup:
    if (lock_created) {
        core->lock_destroy(core->lock_handle);
        core->lock_handle = NULL;
    }
    free(core->heap);
    memset(db, 0, sizeof(*db));
    return err;
}

lox_err_t lox_flush(lox_t *db) {
    lox_core_t *core;
    lox_err_t status;

    if (db == NULL) {
        return LOX_ERR_INVALID;
    }
    LOX_LOCK(db);
    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        LOX_UNLOCK(db);
        return status;
    }

    core = lox_core(db);
    if (core->storage_faulted) {
        LOX_UNLOCK(db);
        return LOX_ERR_INDETERMINATE;
    }
    status = lox_storage_flush(db);
    lox_record_error(core, status);
    LOX_UNLOCK(db);
    return status;
}

lox_err_t lox_deinit(lox_t *db) {
    lox_core_t *core;
    uint8_t *heap;
    void (*lock_destroy)(void *hdl);
    void *lock_handle;
    lox_err_t status;

    if (db == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }
    status = core->storage_faulted ? LOX_ERR_INDETERMINATE : lox_storage_flush(db);
    core->txn_active = 0u;
    core->txn_stage_count = 0u;
    core->txn_active_id = 0u;
    heap = core->heap;
    lock_destroy = core->lock_destroy;
    lock_handle = core->lock_handle;
    core->magic = 0u;
    LOX_UNLOCK(db);

    if (lock_destroy != NULL) {
        lock_destroy(lock_handle);
    }
    lox_record_error(core, status);
    free(heap);
    memset(db, 0, sizeof(*db));
    return status;
}

lox_err_t lox_stats(const lox_t *db, lox_stats_t *out) {
    return lox_inspect((lox_t *)db, out);
}

lox_err_t lox_inspect(lox_t *db, lox_stats_t *out) {
    const lox_core_t *core;
    uint32_t ts_capacity_total = 0u;
    uint32_t ts_samples_total = 0u;
    uint32_t rel_rows_total = 0u;
    uint32_t wal_bytes_used = 0u;
    uint32_t wal_bytes_total = 0u;
    uint32_t i;
    lox_err_t status;

    if (out == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));

    out->kv_entries_used = core->kv.entry_count;
    out->kv_entries_max = (LOX_KV_MAX_KEYS > LOX_TXN_STAGE_KEYS) ? (LOX_KV_MAX_KEYS - LOX_TXN_STAGE_KEYS) : 0u;
    out->kv_fill_pct = lox_fill_pct_u32(out->kv_entries_used, out->kv_entries_max);
    out->kv_collision_count = core->kv.collision_count;
    out->kv_eviction_count = core->kv.eviction_count;

    out->ts_streams_registered = core->ts.registered_streams;
    for (i = 0u; i < LOX_TS_MAX_STREAMS; ++i) {
        ts_samples_total += core->ts.streams[i].count;
        ts_capacity_total += core->ts.streams[i].capacity;
    }
    out->ts_samples_total = ts_samples_total;
    out->ts_fill_pct = lox_fill_pct_u32(ts_samples_total, ts_capacity_total);

    {
        uint32_t header_bytes = lox_wal_header_bytes(core);
        if (core->wal_enabled && core->layout.wal_size > header_bytes) {
            wal_bytes_total = core->layout.wal_size - header_bytes;
            wal_bytes_used = (core->wal_used > header_bytes) ? (core->wal_used - header_bytes) : 0u;
        }
    }
    out->wal_bytes_total = wal_bytes_total;
    out->wal_bytes_used = wal_bytes_used;
    out->wal_fill_pct = lox_fill_pct_u32(wal_bytes_used, wal_bytes_total);

    out->rel_tables_count = core->rel.registered_tables;
    for (i = 0u; i < LOX_REL_MAX_TABLES; ++i) {
        if (core->rel.tables[i].registered) {
            rel_rows_total += core->rel.tables[i].live_count;
        }
    }
    out->rel_rows_total = rel_rows_total;

    LOX_UNLOCK(db);
    return LOX_OK;
}

lox_err_t lox_get_db_stats(lox_t *db, lox_db_stats_t *out) {
    const lox_core_t *core;
    uint32_t wal_bytes_used = 0u;
    uint32_t wal_bytes_total = 0u;
    lox_err_t status;

    if (out == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));
    {
        uint32_t header_bytes = lox_wal_header_bytes(core);
        if (core->wal_enabled && core->layout.wal_size > header_bytes) {
            wal_bytes_total = core->layout.wal_size - header_bytes;
            wal_bytes_used = (core->wal_used > header_bytes) ? (core->wal_used - header_bytes) : 0u;
        }
    }
    out->effective_capacity_bytes = (core->storage != NULL) ? core->storage->capacity : 0u;
    out->wal_bytes_total = wal_bytes_total;
    out->wal_bytes_used = wal_bytes_used;
    out->wal_fill_pct = lox_fill_pct_u32(wal_bytes_used, wal_bytes_total);
    out->compact_count = core->compact_count;
    out->reopen_count = core->reopen_count;
    out->recovery_count = core->recovery_count;
    out->last_runtime_error = core->last_runtime_error;
    out->last_recovery_status = core->last_recovery_status;
    out->recovery_detail = core->recovery_detail;
    out->active_generation = core->layout.active_generation;
    out->active_bank = core->layout.active_bank;

    LOX_UNLOCK(db);
    return LOX_OK;
}

lox_err_t lox_get_kv_stats(lox_t *db, lox_kv_stats_t *out) {
    const lox_core_t *core;
    lox_err_t status;
    uint32_t entry_limit;

    if (out == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }

    entry_limit = (LOX_KV_MAX_KEYS > LOX_TXN_STAGE_KEYS) ? (LOX_KV_MAX_KEYS - LOX_TXN_STAGE_KEYS) : 0u;
    memset(out, 0, sizeof(*out));
    out->live_keys = core->kv.entry_count;
    out->collisions = core->kv.collision_count;
    out->evictions = core->kv.eviction_count;
    out->tombstones = lox_kv_tombstone_count(core);
    out->value_bytes_used = core->kv.value_used;
    out->fill_pct = lox_fill_pct_u32(out->live_keys, entry_limit);

    LOX_UNLOCK(db);
    return LOX_OK;
}

lox_err_t lox_get_ts_stats(lox_t *db, lox_ts_stats_t *out) {
    const lox_core_t *core;
    lox_err_t status;
    uint32_t ts_capacity_total = 0u;
    uint32_t ts_samples_total = 0u;
    uint32_t i;

    if (out == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));
    out->stream_count = core->ts.registered_streams;
    for (i = 0u; i < LOX_TS_MAX_STREAMS; ++i) {
        ts_samples_total += core->ts.streams[i].count;
        ts_capacity_total += core->ts.streams[i].capacity;
    }
    out->retained_samples = ts_samples_total;
    out->dropped_samples = core->ts_dropped_samples;
    out->fill_pct = lox_fill_pct_u32(ts_samples_total, ts_capacity_total);

    LOX_UNLOCK(db);
    return LOX_OK;
}

lox_err_t lox_get_rel_stats(lox_t *db, lox_rel_stats_t *out) {
    const lox_core_t *core;
    lox_err_t status;
    uint32_t i;
    uint32_t rows_live = 0u;
    uint32_t rows_capacity = 0u;
    uint32_t indexed_tables = 0u;
    uint32_t index_entries = 0u;

    if (out == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));
    out->table_count = core->rel.registered_tables;
    for (i = 0u; i < LOX_REL_MAX_TABLES; ++i) {
        const lox_table_t *table = &core->rel.tables[i];
        if (!table->registered) {
            continue;
        }
        rows_live += table->live_count;
        rows_capacity += table->max_rows;
        if (table->index_col != UINT32_MAX) {
            indexed_tables++;
            index_entries += table->index_count;
        }
    }
    out->rows_live = rows_live;
    out->rows_free = (rows_capacity > rows_live) ? (rows_capacity - rows_live) : 0u;
    out->indexed_tables = indexed_tables;
    out->index_entries = index_entries;

    LOX_UNLOCK(db);
    return LOX_OK;
}

lox_err_t lox_get_effective_capacity(lox_t *db, lox_effective_capacity_t *out) {
    const lox_core_t *core;
    lox_storage_layout_t storage_layout;
    uint32_t storage_required;
    lox_err_t status;
    uint32_t ts_retained = 0u;
    uint32_t ts_total = 0u;
    uint32_t i;
    uint32_t entry_limit;
    uint32_t kv_free_now;
    uint32_t wal_total = 0u;
    uint32_t wal_used = 0u;
    uint32_t wal_free = 0u;
    uint32_t threshold_pct = 0u;

    if (out == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));
    storage_layout = core->layout;
    if (core->storage != NULL) {
        lox_storage_layout_t checked_layout;
        if (lox_compute_storage_layout(core->storage,
                                       core->ts_arena.capacity,
                                       core->rel_arena.capacity,
                                       8u,
                                       &checked_layout,
                                       &storage_required) == LOX_OK) {
            storage_layout = checked_layout;
        }
    }
    entry_limit = (LOX_KV_MAX_KEYS > LOX_TXN_STAGE_KEYS) ? (LOX_KV_MAX_KEYS - LOX_TXN_STAGE_KEYS) : 0u;
    out->kv_entries_usable = entry_limit;
    out->kv_entries_free = (entry_limit > core->kv.entry_count) ? (entry_limit - core->kv.entry_count) : 0u;
    out->kv_value_bytes_usable = core->kv.value_capacity;
    kv_free_now = (core->kv.value_capacity > core->kv.value_used) ? (core->kv.value_capacity - core->kv.value_used) : 0u;
    out->kv_value_bytes_free_now = kv_free_now;

    for (i = 0u; i < LOX_TS_MAX_STREAMS; ++i) {
        ts_retained += core->ts.streams[i].count;
        ts_total += core->ts.streams[i].capacity;
    }
    out->ts_samples_usable = ts_total;
    out->ts_samples_retained = ts_retained;
    out->ts_samples_free = (ts_total > ts_retained) ? (ts_total - ts_retained) : 0u;

    {
        uint32_t header_bytes = lox_wal_header_bytes(core);
        if (core->wal_enabled && storage_layout.wal_size > header_bytes) {
            wal_total = storage_layout.wal_size - header_bytes;
            wal_used = (core->wal_used > header_bytes) ? (core->wal_used - header_bytes) : 0u;
            wal_free = (wal_total > wal_used) ? (wal_total - wal_used) : 0u;
        }
    }
    out->wal_budget_total = wal_total;
    out->wal_budget_used = wal_used;
    out->wal_budget_free = wal_free;
    threshold_pct = (core->wal_compact_threshold_pct != 0u) ? core->wal_compact_threshold_pct : 75u;
    out->compact_threshold_pct = threshold_pct;
    out->wal_safety_reserved = (wal_total * threshold_pct) / 100u;

    if (core->storage == NULL) {
        out->limiting_flags |= LOX_CAP_LIMIT_STORAGE_DISABLED;
    }
    if (out->kv_entries_free == 0u) {
        out->limiting_flags |= LOX_CAP_LIMIT_KV_ENTRIES;
    }
    if (out->kv_value_bytes_free_now == 0u) {
        out->limiting_flags |= LOX_CAP_LIMIT_KV_VALUE_BYTES;
    }
    if (out->ts_samples_free == 0u) {
        out->limiting_flags |= LOX_CAP_LIMIT_TS_SAMPLES;
    }
    if (out->wal_budget_total != 0u && out->wal_budget_free == 0u) {
        out->limiting_flags |= LOX_CAP_LIMIT_WAL_BUDGET;
    }

    LOX_UNLOCK(db);
    return LOX_OK;
}

lox_err_t lox_get_pressure(lox_t *db, lox_pressure_t *out) {
    const lox_core_t *core;
    lox_err_t status;
    uint32_t ts_total = 0u;
    uint32_t ts_retained = 0u;
    uint32_t rel_rows_live = 0u;
    uint32_t rel_rows_capacity = 0u;
    uint32_t wal_total = 0u;
    uint32_t wal_used = 0u;
    uint32_t threshold_pct = 0u;
    uint32_t i;
    uint32_t max_risk;

    if (out == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));
    out->kv_fill_pct = lox_fill_pct_u32(core->kv.entry_count,
                                            (LOX_KV_MAX_KEYS > LOX_TXN_STAGE_KEYS)
                                                ? (LOX_KV_MAX_KEYS - LOX_TXN_STAGE_KEYS)
                                                : 0u);

    for (i = 0u; i < LOX_TS_MAX_STREAMS; ++i) {
        ts_total += core->ts.streams[i].capacity;
        ts_retained += core->ts.streams[i].count;
    }
    out->ts_fill_pct = lox_fill_pct_u32(ts_retained, ts_total);

    for (i = 0u; i < LOX_REL_MAX_TABLES; ++i) {
        const lox_table_t *table = &core->rel.tables[i];
        if (!table->registered) {
            continue;
        }
        rel_rows_live += table->live_count;
        rel_rows_capacity += table->max_rows;
    }
    out->rel_fill_pct = lox_fill_pct_u32(rel_rows_live, rel_rows_capacity);

    {
        uint32_t header_bytes = lox_wal_header_bytes(core);
        if (core->wal_enabled && core->layout.wal_size > header_bytes) {
            wal_total = core->layout.wal_size - header_bytes;
            wal_used = (core->wal_used > header_bytes) ? (core->wal_used - header_bytes) : 0u;
        }
    }
    out->wal_fill_pct = lox_fill_pct_u32(wal_used, wal_total);

    threshold_pct = (core->wal_compact_threshold_pct != 0u) ? core->wal_compact_threshold_pct : 75u;
    if (wal_total == 0u) {
        out->compact_pressure_pct = 0u;
    } else if (threshold_pct == 0u) {
        out->compact_pressure_pct = out->wal_fill_pct;
    } else {
        uint32_t pressure = (uint32_t)out->wal_fill_pct * 100u / threshold_pct;
        out->compact_pressure_pct = (uint8_t)((pressure > 100u) ? 100u : pressure);
    }

    out->risk_flags = LOX_CAP_LIMIT_NONE;
    if (core->storage == NULL) {
        out->risk_flags |= LOX_CAP_LIMIT_STORAGE_DISABLED;
    }
    if (out->kv_fill_pct >= 100u) {
        out->risk_flags |= LOX_CAP_LIMIT_KV_ENTRIES;
    }
    if (out->ts_fill_pct >= 100u) {
        out->risk_flags |= LOX_CAP_LIMIT_TS_SAMPLES;
    }
    if (out->wal_fill_pct >= 100u) {
        out->risk_flags |= LOX_CAP_LIMIT_WAL_BUDGET;
    }

    max_risk = out->kv_fill_pct;
    if (out->ts_fill_pct > max_risk) {
        max_risk = out->ts_fill_pct;
    }
    if (out->rel_fill_pct > max_risk) {
        max_risk = out->rel_fill_pct;
    }
    if (out->wal_fill_pct > max_risk) {
        max_risk = out->wal_fill_pct;
    }
    if (out->compact_pressure_pct > max_risk) {
        max_risk = out->compact_pressure_pct;
    }
    out->near_full_risk_pct = (uint8_t)max_risk;

    LOX_UNLOCK(db);
    return LOX_OK;
}

lox_err_t lox_selfcheck(lox_t *db, lox_selfcheck_result_t *out) {
    lox_core_t *core;
    lox_err_t status;
    uint32_t i;
    uint8_t wal_ok = 1u;

    if (db == NULL || out == NULL) {
        return LOX_ERR_INVALID;
    }

    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        return status;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        return LOX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));
    out->kv_ok = 1u;
    out->ts_ok = 1u;
    out->rel_ok = 1u;
    out->wal_ok = 1u;

    {
        uint32_t live_count = 0u;
        uint32_t live_value_bytes = 0u;

        for (i = 0u; i < core->kv.bucket_count; ++i) {
            const lox_kv_bucket_t *b = &core->kv.buckets[i];
            if (b->state != 1u) {
                continue;
            }
            live_count++;
            live_value_bytes += b->val_len;
            if (b->val_offset + b->val_len > core->kv.value_capacity) {
                out->kv_anomalies++;
                out->kv_ok = 0u;
                lox_selfcheck_set_first(out, "kv: value range out of capacity");
            }
        }
        if (live_count != core->kv.entry_count) {
            out->kv_anomalies++;
            out->kv_ok = 0u;
            lox_selfcheck_set_first(out, "kv: entry_count mismatch");
        }
        if (live_value_bytes != core->kv.live_value_bytes) {
            out->kv_anomalies++;
            out->kv_ok = 0u;
            lox_selfcheck_set_first(out, "kv: live_value_bytes mismatch");
        }
        for (i = 0u; i < core->kv.bucket_count; ++i) {
            const lox_kv_bucket_t *a = &core->kv.buckets[i];
            uint32_t j;
            if (a->state != 1u || a->val_len == 0u) {
                continue;
            }
            for (j = i + 1u; j < core->kv.bucket_count; ++j) {
                const lox_kv_bucket_t *b = &core->kv.buckets[j];
                uint32_t a0;
                uint32_t a1;
                uint32_t b0;
                uint32_t b1;
                if (b->state != 1u || b->val_len == 0u) {
                    continue;
                }
                a0 = a->val_offset;
                a1 = a->val_offset + a->val_len;
                b0 = b->val_offset;
                b1 = b->val_offset + b->val_len;
                if (a0 < b1 && b0 < a1) {
                    out->kv_anomalies++;
                    out->kv_ok = 0u;
                    lox_selfcheck_set_first(out, "kv: overlapping value ranges");
                }
            }
        }
    }

#if LOX_ENABLE_TS
    {
        uint32_t registered = 0u;
        for (i = 0u; i < LOX_TS_MAX_STREAMS; ++i) {
            const lox_ts_stream_t *s = &core->ts.streams[i];
            if (!s->registered) {
                continue;
            }
            registered++;
            if (s->count > s->capacity) {
                out->ts_anomalies++;
                out->ts_ok = 0u;
                lox_selfcheck_set_first(out, "ts: count > capacity");
            }
            if (s->count > 0u && s->head >= s->capacity) {
                out->ts_anomalies++;
                out->ts_ok = 0u;
                lox_selfcheck_set_first(out, "ts: head out of range");
            }
        }
        if (registered != core->ts.registered_streams) {
            out->ts_anomalies++;
            out->ts_ok = 0u;
            lox_selfcheck_set_first(out, "ts: registered stream count mismatch");
        }
    }
#endif

#if LOX_ENABLE_REL
    {
        uint32_t registered_tables = 0u;
        for (i = 0u; i < LOX_REL_MAX_TABLES; ++i) {
            const lox_table_t *t = &core->rel.tables[i];
            uint32_t alive = 0u;
            uint32_t b;
            if (!t->registered) {
                continue;
            }
            registered_tables++;
            for (b = 0u; b < (t->max_rows + 7u) / 8u; ++b) {
                alive += lox_popcount8(t->alive_bitmap[b]);
            }
            if (alive != t->live_count) {
                out->rel_anomalies++;
                out->rel_ok = 0u;
                lox_selfcheck_set_first(out, "rel: live_count bitmap mismatch");
            }
            if (t->index_count > t->live_count) {
                out->rel_anomalies++;
                out->rel_ok = 0u;
                lox_selfcheck_set_first(out, "rel: index_count > live_count");
            }
            if (t->index_count > 1u && t->index != NULL && t->index_key_size > 0u) {
                uint32_t k;
                for (k = 1u; k < t->index_count; ++k) {
                    const uint8_t *prev = t->index[k - 1u].key_bytes;
                    const uint8_t *cur = t->index[k].key_bytes;
                    if (memcmp(prev, cur, t->index_key_size) > 0) {
                        out->rel_anomalies++;
                        out->rel_ok = 0u;
                        lox_selfcheck_set_first(out, "rel: index not sorted");
                        break;
                    }
                }
            }
        }
        if (registered_tables != core->rel.registered_tables) {
            out->rel_anomalies++;
            out->rel_ok = 0u;
            lox_selfcheck_set_first(out, "rel: registered table count mismatch");
        }
    }
#endif

    if (core->magic != LOX_MAGIC) {
        wal_ok = 0u;
        out->wal_ok = 0u;
        lox_selfcheck_set_first(out, "wal: invalid handle magic");
    }
    if (core->wal_enabled && core->wal_used > core->layout.wal_size) {
        wal_ok = 0u;
        out->wal_ok = 0u;
        lox_selfcheck_set_first(out, "wal: wal_used exceeds wal_size");
    }

    LOX_UNLOCK(db);

    if (out->kv_anomalies > 0u || out->ts_anomalies > 0u || out->rel_anomalies > 0u || wal_ok == 0u) {
        return LOX_ERR_CORRUPT;
    }
    return LOX_OK;
}

lox_err_t lox_admit_kv_set(lox_t *db, const char *key, size_t val_len, lox_admission_t *out) {
    if (out == NULL || key == NULL || key[0] == '\0') {
        return LOX_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));

#if !LOX_ENABLE_KV
    (void)db;
    (void)val_len;
    out->status = LOX_ERR_DISABLED;
    return LOX_ERR_DISABLED;
#else
    const lox_core_t *core;
    lox_err_t status;
    uint32_t required = 0u;
    uint32_t available = 0u;
    uint32_t compact_available = 0u;
    uint32_t entry_limit;
    uint8_t would_compact = 0u;
    uint32_t wal_free = 0u;
    uint32_t payload_len = 0u;
    uint32_t wal_bytes = 0u;
    const lox_kv_bucket_t *existing;
    if (val_len > LOX_KV_VAL_MAX_LEN || strlen(key) >= LOX_KV_KEY_MAX_LEN) {
        out->status = LOX_ERR_INVALID;
        return LOX_ERR_INVALID;
    }
    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        out->status = status;
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        out->status = LOX_ERR_INVALID;
        return LOX_ERR_INVALID;
    }

    existing = lox_kv_find_bucket_const(core, key);
    if (existing != NULL) {
        required = (val_len > existing->val_len) ? (uint32_t)(val_len - existing->val_len) : 0u;
    } else {
        required = (uint32_t)val_len;
        entry_limit = (LOX_KV_MAX_KEYS > LOX_TXN_STAGE_KEYS) ? (LOX_KV_MAX_KEYS - LOX_TXN_STAGE_KEYS) : 0u;
        if (core->kv.entry_count >= entry_limit) {
#if LOX_KV_OVERFLOW_POLICY == LOX_KV_POLICY_REJECT
            out->status = LOX_ERR_FULL;
            out->deterministic_budget_ok = 0u;
            LOX_UNLOCK(db);
            return LOX_OK;
#else
            out->would_degrade = 1u;
            out->deterministic_budget_ok = 0u;
#endif
        }
    }

    available = (core->kv.value_capacity > core->kv.value_used) ? (core->kv.value_capacity - core->kv.value_used) : 0u;
    compact_available = (core->kv.value_capacity > lox_kv_live_value_bytes_local(core)) ?
                            (core->kv.value_capacity - lox_kv_live_value_bytes_local(core)) :
                            0u;
    out->required_bytes = required;
    out->available_bytes = available;

    if (required > available) {
        if (required <= compact_available) {
            out->would_compact = 1u;
        } else {
            out->status = LOX_ERR_NO_MEM;
            out->deterministic_budget_ok = 0u;
            LOX_UNLOCK(db);
            return LOX_OK;
        }
    }

    if (core->wal_enabled) {
        size_t payload_size = 0u;
        if (!lox_checked_add_size(1u, strlen(key), &payload_size) ||
            !lox_checked_add_size(payload_size, 4u, &payload_size) ||
            !lox_checked_add_size(payload_size, val_len, &payload_size) ||
            !lox_checked_add_size(payload_size, 8u, &payload_size) ||
            !lox_checked_u32_from_size(payload_size, &payload_len)) {
            LOX_UNLOCK(db);
            return LOX_ERR_OVERFLOW;
        }
        wal_bytes = lox_wal_entry_size_for_payload(payload_len);
        out->required_wal_bytes = wal_bytes;
        lox_fill_wal_admission(core, wal_bytes, &would_compact, &wal_free);
        out->wal_bytes_free = wal_free;
        if (would_compact != 0u) {
            out->would_compact = 1u;
        }
    }

    if (out->deterministic_budget_ok == 0u && out->would_degrade == 0u) {
        out->deterministic_budget_ok = 1u;
    }
    out->status = LOX_OK;
    LOX_UNLOCK(db);
    return LOX_OK;
#endif
}

lox_err_t lox_admit_ts_insert(lox_t *db, const char *stream_name, size_t sample_len, lox_admission_t *out) {
    if (out == NULL || stream_name == NULL || stream_name[0] == '\0') {
        return LOX_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));

#if !LOX_ENABLE_TS
    (void)db;
    (void)sample_len;
    out->status = LOX_ERR_DISABLED;
    return LOX_ERR_DISABLED;
#else
    const lox_core_t *core;
    const lox_ts_stream_t *stream;
    lox_err_t status;
    uint32_t expected_len = 0u;
    uint8_t would_compact = 0u;
    uint32_t wal_free = 0u;
    uint32_t wal_bytes = 0u;
    uint32_t payload_len = 0u;
    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        out->status = status;
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        out->status = LOX_ERR_INVALID;
        return LOX_ERR_INVALID;
    }

    stream = lox_ts_find_const(core, stream_name);
    if (stream == NULL) {
        out->status = LOX_ERR_NOT_FOUND;
        LOX_UNLOCK(db);
        return LOX_OK;
    }
    expected_len = (stream->type == LOX_TS_RAW) ? (uint32_t)stream->raw_size : 4u;
    if (sample_len != expected_len) {
        out->status = LOX_ERR_INVALID;
        LOX_UNLOCK(db);
        return LOX_OK;
    }

    out->required_bytes = 1u;
    out->available_bytes = (stream->capacity > stream->count) ? (stream->capacity - stream->count) : 0u;
    if (stream->count >= stream->capacity) {
#if LOX_TS_OVERFLOW_POLICY == LOX_TS_POLICY_REJECT
        out->status = LOX_ERR_FULL;
        out->deterministic_budget_ok = 0u;
        LOX_UNLOCK(db);
        return LOX_OK;
#else
        out->would_degrade = 1u;
        out->deterministic_budget_ok = 0u;
#endif
    }

    if (core->wal_enabled) {
        payload_len = (uint32_t)(1u + strlen(stream_name) + 9u + sample_len);
        wal_bytes = lox_wal_entry_size_for_payload(payload_len);
        out->required_wal_bytes = wal_bytes;
        lox_fill_wal_admission(core, wal_bytes, &would_compact, &wal_free);
        out->wal_bytes_free = wal_free;
        if (would_compact != 0u) {
            out->would_compact = 1u;
        }
    }

    if (out->deterministic_budget_ok == 0u && out->would_degrade == 0u) {
        out->deterministic_budget_ok = 1u;
    }
    out->status = LOX_OK;
    LOX_UNLOCK(db);
    return LOX_OK;
#endif
}

lox_err_t lox_admit_rel_insert(lox_t *db, const char *table_name, size_t row_len, lox_admission_t *out) {
    if (out == NULL || table_name == NULL || table_name[0] == '\0') {
        return LOX_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));

#if !LOX_ENABLE_REL
    (void)db;
    (void)row_len;
    out->status = LOX_ERR_DISABLED;
    return LOX_ERR_DISABLED;
#else
    const lox_core_t *core;
    const lox_table_t *table;
    lox_err_t status;
    uint8_t would_compact = 0u;
    uint32_t wal_free = 0u;
    uint32_t wal_bytes = 0u;
    uint32_t payload_len = 0u;
    status = lox_validate_handle(db);
    if (status != LOX_OK) {
        out->status = status;
        return status;
    }

    LOX_LOCK(db);
    core = lox_core_const(db);
    if (core->magic != LOX_MAGIC) {
        LOX_UNLOCK(db);
        out->status = LOX_ERR_INVALID;
        return LOX_ERR_INVALID;
    }

    table = lox_rel_find_table_const(core, table_name);
    if (table == NULL) {
        out->status = LOX_ERR_NOT_FOUND;
        LOX_UNLOCK(db);
        return LOX_OK;
    }
    if (row_len != table->row_size) {
        out->status = LOX_ERR_INVALID;
        LOX_UNLOCK(db);
        return LOX_OK;
    }

    out->required_bytes = 1u;
    out->available_bytes = (table->max_rows > table->live_count) ? (table->max_rows - table->live_count) : 0u;
    if (table->live_count >= table->max_rows) {
        out->status = LOX_ERR_FULL;
        out->deterministic_budget_ok = 0u;
        LOX_UNLOCK(db);
        return LOX_OK;
    }

    if (core->wal_enabled) {
        payload_len = (uint32_t)(1u + strlen(table_name) + 4u + row_len);
        wal_bytes = lox_wal_entry_size_for_payload(payload_len);
        out->required_wal_bytes = wal_bytes;
        lox_fill_wal_admission(core, wal_bytes, &would_compact, &wal_free);
        out->wal_bytes_free = wal_free;
        if (would_compact != 0u) {
            out->would_compact = 1u;
        }
    }

    if (out->would_compact != 0u || out->would_degrade != 0u) {
        out->deterministic_budget_ok = 0u;
    } else if (out->status == LOX_OK &&
               out->deterministic_budget_ok == 0u &&
               out->would_degrade == 0u) {
        out->deterministic_budget_ok = 1u;
    }
    out->status = LOX_OK;
    LOX_UNLOCK(db);
    return LOX_OK;
#endif
}
