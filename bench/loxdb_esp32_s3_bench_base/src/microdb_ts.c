// SPDX-License-Identifier: MIT
#include "lox_internal.h"
#include "lox_lock.h"

#include <string.h>

static lox_err_t lox_ts_validate_name(const char *name) {
    size_t len;

    if (name == NULL || name[0] == '\0') {
        return LOX_ERR_INVALID;
    }

    len = strlen(name);
    if (len >= LOX_TS_STREAM_NAME_LEN) {
        return LOX_ERR_INVALID;
    }

    return LOX_OK;
}

static lox_ts_stream_t *lox_ts_find(lox_core_t *core, const char *name) {
    uint32_t i;

    for (i = 0; i < LOX_TS_MAX_STREAMS; ++i) {
        lox_ts_stream_t *stream = &core->ts.streams[i];
        if (stream->registered && strcmp(stream->name, name) == 0) {
            return stream;
        }
    }

    return NULL;
}

static lox_err_t lox_ts_register_apply(lox_core_t *core,
                                               const char *name,
                                               lox_ts_type_t type,
                                               size_t raw_size) {
    uint32_t i;

    if (lox_ts_find(core, name) != NULL) {
        return LOX_ERR_EXISTS;
    }
    if (core->ts.registered_streams >= LOX_TS_MAX_STREAMS) {
        return LOX_ERR_FULL;
    }

    for (i = 0; i < LOX_TS_MAX_STREAMS; ++i) {
        lox_ts_stream_t *stream = &core->ts.streams[i];
        if (!stream->registered) {
            memset(stream->name, 0, sizeof(stream->name));
            memcpy(stream->name, name, strlen(name) + 1u);
            stream->type = type;
            stream->raw_size = (type == LOX_TS_RAW) ? raw_size : 0u;
            stream->head = 0u;
            stream->tail = 0u;
            stream->count = 0u;
            stream->registered = true;
            core->ts.registered_streams++;
            return LOX_OK;
        }
    }

    return LOX_ERR_FULL;
}

static lox_err_t lox_ts_sample_capacity(const lox_core_t *core, uint32_t *capacity_out) {
    size_t bytes_per_stream;
    uint32_t capacity;

    bytes_per_stream = core->ts_arena.capacity / LOX_TS_MAX_STREAMS;
    capacity = (uint32_t)(bytes_per_stream / sizeof(lox_ts_sample_t));
    if (capacity < 4u) {
        return LOX_ERR_NO_MEM;
    }

    *capacity_out = capacity;
    return LOX_OK;
}

static void lox_ts_set_value(lox_ts_stream_t *stream, lox_ts_sample_t *sample, const void *val) {
    if (stream->type == LOX_TS_F32) {
        sample->v.f32 = *(const float *)val;
    } else if (stream->type == LOX_TS_I32) {
        sample->v.i32 = *(const int32_t *)val;
    } else if (stream->type == LOX_TS_U32) {
        sample->v.u32 = *(const uint32_t *)val;
    } else {
        memcpy(sample->v.raw, val, stream->raw_size);
    }
}

static void lox_ts_rb_insert(lox_ts_stream_t *stream, const lox_ts_sample_t *sample) {
    if (stream->count == stream->capacity) {
#if LOX_TS_OVERFLOW_POLICY == LOX_TS_POLICY_DROP_OLDEST
        LOX_LOG("WARN",
                    "TS stream '%s' full: dropping oldest sample ts=%u",
                    stream->name,
                    (unsigned)stream->buf[stream->tail].ts);
#endif
    }

    stream->buf[stream->head] = *sample;
    stream->head = (stream->head + 1u) % stream->capacity;

    if (stream->count < stream->capacity) {
        stream->count++;
    } else {
        stream->tail = (stream->tail + 1u) % stream->capacity;
    }
}

static void lox_ts_downsample_oldest(lox_ts_stream_t *stream) {
    uint32_t i0 = stream->tail;
    uint32_t i1 = (stream->tail + 1u) % stream->capacity;
    uint32_t idx;
    uint32_t next;
    lox_ts_sample_t *a = &stream->buf[i0];
    lox_ts_sample_t *b = &stream->buf[i1];

    LOX_LOG("INFO",
                "TS stream '%s' downsampling oldest two samples",
                stream->name);

    a->ts = (a->ts / 2u) + (b->ts / 2u);

    if (stream->type == LOX_TS_F32) {
        a->v.f32 = (a->v.f32 + b->v.f32) * 0.5f;
    } else if (stream->type == LOX_TS_I32) {
        a->v.i32 = (a->v.i32 / 2) + (b->v.i32 / 2);
    } else if (stream->type == LOX_TS_U32) {
        a->v.u32 = (a->v.u32 / 2u) + (b->v.u32 / 2u);
    }

    idx = i1;
    while (idx != stream->head) {
        next = (idx + 1u) % stream->capacity;
        if (next == stream->head) {
            break;
        }
        stream->buf[idx] = stream->buf[next];
        idx = next;
    }

    stream->head = (stream->head + stream->capacity - 1u) % stream->capacity;
    stream->count--;
}

lox_err_t lox_ts_init(lox_t *db) {
    lox_core_t *core = lox_core(db);
#if LOX_ENABLE_TS
    uint32_t capacity;
    uint32_t i;
#endif

    memset(&core->ts, 0, sizeof(core->ts));

#if LOX_ENABLE_TS
    if (lox_ts_sample_capacity(core, &capacity) != LOX_OK) {
        return LOX_ERR_NO_MEM;
    }

    for (i = 0; i < LOX_TS_MAX_STREAMS; ++i) {
        core->ts.streams[i].buf =
            (lox_ts_sample_t *)(core->ts_arena.base + (i * capacity * sizeof(lox_ts_sample_t)));
        core->ts.streams[i].capacity = capacity;
    }
#endif

    return LOX_OK;
}

#if LOX_ENABLE_TS
lox_err_t lox_ts_register(lox_t *db, const char *name, lox_ts_type_t type, size_t raw_size) {
    lox_core_t *core;
    lox_err_t err;
    lox_err_t rc = LOX_OK;
    uint32_t before_registered = 0u;
    uint32_t restore_index = UINT32_MAX;
    lox_ts_stream_t restore_stream;

    if (db == NULL) {
        return LOX_ERR_INVALID;
    }

    err = lox_ts_validate_name(name);
    if (err != LOX_OK) {
        return err;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        rc = LOX_ERR_INVALID;
        goto unlock;
    }

    if (type == LOX_TS_RAW && (raw_size == 0u || raw_size > LOX_TS_RAW_MAX)) {
        rc = LOX_ERR_INVALID;
        goto unlock;
    }

    if (core->wal_enabled && core->storage != NULL && !core->storage_loading && !core->wal_replaying) {
        rc = lox_persist_ts_register(db, name, type, raw_size);
        if (rc != LOX_OK) {
            goto unlock;
        }
        rc = lox_ts_register_apply(core, name, type, raw_size);
        goto unlock;
    }

    before_registered = core->ts.registered_streams;
    memset(&restore_stream, 0, sizeof(restore_stream));
    {
        uint32_t i;
        for (i = 0u; i < LOX_TS_MAX_STREAMS; ++i) {
            if (!core->ts.streams[i].registered) {
                restore_index = i;
                restore_stream = core->ts.streams[i];
                break;
            }
        }
    }
    rc = lox_ts_register_apply(core, name, type, raw_size);
    if (rc != LOX_OK) {
        goto unlock;
    }
    rc = lox_storage_flush(db);
    if (rc != LOX_OK) {
        core->ts.registered_streams = before_registered;
        if (restore_index != UINT32_MAX) {
            core->ts.streams[restore_index] = restore_stream;
        }
    }

unlock:
    LOX_UNLOCK(db);
    return rc;
}

lox_err_t lox_ts_insert(lox_t *db, const char *name, lox_timestamp_t ts, const void *val) {
    lox_core_t *core;
    lox_ts_stream_t *stream;
    lox_ts_sample_t sample;
    lox_err_t rc = LOX_OK;

    if (db == NULL || val == NULL) {
        return LOX_ERR_INVALID;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        rc = LOX_ERR_INVALID;
        goto unlock;
    }

    stream = lox_ts_find(core, name);
    if (stream == NULL) {
        rc = LOX_ERR_NOT_FOUND;
        goto unlock;
    }

    if (stream->capacity == 0u) {
        rc = LOX_ERR_NO_MEM;
        goto unlock;
    }

#if LOX_TS_OVERFLOW_POLICY == LOX_TS_POLICY_REJECT
    if (stream->count == stream->capacity) {
        LOX_LOG("WARN",
                    "TS stream '%s' full: rejecting new sample (REJECT policy)",
                    stream->name);
        rc = LOX_ERR_FULL;
        goto unlock;
    }
#elif LOX_TS_OVERFLOW_POLICY == LOX_TS_POLICY_DOWNSAMPLE
#endif

    memset(&sample, 0, sizeof(sample));
    sample.ts = ts;
    lox_ts_set_value(stream, &sample, val);
    rc = lox_persist_ts_insert(db, name, ts, val, (stream->type == LOX_TS_RAW) ? stream->raw_size : 4u);
    if (rc == LOX_OK) {
#if LOX_TS_OVERFLOW_POLICY == LOX_TS_POLICY_DOWNSAMPLE
        if (stream->count == stream->capacity) {
            lox_ts_downsample_oldest(stream);
        }
#endif
        lox_ts_rb_insert(stream, &sample);
        lox__maybe_compact(db);
    }

unlock:
    LOX_UNLOCK(db);
    return rc;
}

lox_err_t lox_ts_last(lox_t *db, const char *name, lox_ts_sample_t *out) {
    lox_core_t *core;
    lox_ts_stream_t *stream;
    uint32_t idx;
    lox_err_t rc = LOX_OK;

    if (db == NULL || out == NULL) {
        return LOX_ERR_INVALID;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        rc = LOX_ERR_INVALID;
        goto unlock;
    }

    stream = lox_ts_find(core, name);
    if (stream == NULL || stream->count == 0u) {
        rc = LOX_ERR_NOT_FOUND;
        goto unlock;
    }

    idx = (stream->head + stream->capacity - 1u) % stream->capacity;
    *out = stream->buf[idx];
    rc = LOX_OK;

unlock:
    LOX_UNLOCK(db);
    return rc;
}

lox_err_t lox_ts_query(lox_t *db,
                               const char *name,
                               lox_timestamp_t from,
                               lox_timestamp_t to,
                               lox_ts_query_cb_t cb,
                               void *ctx) {
    lox_core_t *core;
    lox_ts_stream_t *stream;
    uint32_t i;
    uint32_t idx;
    lox_err_t rc = LOX_OK;

    if (db == NULL || cb == NULL) {
        return LOX_ERR_INVALID;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        rc = LOX_ERR_INVALID;
        goto unlock;
    }

    stream = lox_ts_find(core, name);
    if (stream == NULL) {
        rc = LOX_ERR_NOT_FOUND;
        goto unlock;
    }

    {
        uint32_t snapshot_tail = stream->tail;
        uint32_t snapshot_count = stream->count;
        uint32_t cap = stream->capacity;
        idx = snapshot_tail;
        for (i = 0u; i < snapshot_count; ++i) {
            lox_ts_sample_t sample = stream->buf[idx];
            bool in_range = (from <= to && sample.ts >= from && sample.ts <= to);
            idx = (idx + 1u) % cap;
            LOX_UNLOCK(db);
            if (in_range && !cb(&sample, ctx)) {
                return LOX_OK;
            }
            LOX_LOCK(db);
            core = lox_core(db);
            if (core->magic != LOX_MAGIC) {
                return LOX_ERR_INVALID;
            }
            stream = lox_ts_find(core, name);
            if (stream == NULL) {
                return LOX_ERR_NOT_FOUND;
            }
        }
    }

    rc = LOX_OK;

unlock:
    LOX_UNLOCK(db);
    return rc;
}

lox_err_t lox_ts_query_buf(lox_t *db,
                                   const char *name,
                                   lox_timestamp_t from,
                                   lox_timestamp_t to,
                                   lox_ts_sample_t *buf,
                                   size_t max_count,
                                   size_t *out_count) {
    lox_core_t *core;
    lox_ts_stream_t *stream;
    uint32_t i;
    uint32_t idx;
    size_t written = 0u;
    lox_err_t status = LOX_OK;

    if (db == NULL || buf == NULL) {
        return LOX_ERR_INVALID;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        status = LOX_ERR_INVALID;
        goto unlock;
    }

    stream = lox_ts_find(core, name);
    if (stream == NULL) {
        status = LOX_ERR_NOT_FOUND;
        goto unlock;
    }

    idx = stream->tail;
    for (i = 0; i < stream->count; ++i) {
        lox_ts_sample_t *sample = &stream->buf[idx];
        if (from <= to && sample->ts >= from && sample->ts <= to) {
            if (written < max_count) {
                buf[written] = *sample;
            } else {
                status = LOX_ERR_OVERFLOW;
            }
            written++;
        }
        idx = (idx + 1u) % stream->capacity;
    }

    if (out_count != NULL) {
        *out_count = (written < max_count) ? written : max_count;
    }

unlock:
    LOX_UNLOCK(db);
    return status;
}

lox_err_t lox_ts_count(lox_t *db,
                               const char *name,
                               lox_timestamp_t from,
                               lox_timestamp_t to,
                               size_t *out_count) {
    lox_core_t *core;
    lox_ts_stream_t *stream;
    uint32_t i;
    uint32_t idx;
    size_t count = 0u;
    lox_err_t rc = LOX_OK;

    if (db == NULL || out_count == NULL) {
        return LOX_ERR_INVALID;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        rc = LOX_ERR_INVALID;
        goto unlock;
    }

    stream = lox_ts_find(core, name);
    if (stream == NULL) {
        rc = LOX_ERR_NOT_FOUND;
        goto unlock;
    }

    idx = stream->tail;
    for (i = 0; i < stream->count; ++i) {
        lox_ts_sample_t *sample = &stream->buf[idx];
        if (from <= to && sample->ts >= from && sample->ts <= to) {
            count++;
        }
        idx = (idx + 1u) % stream->capacity;
    }

    *out_count = count;
    rc = LOX_OK;

unlock:
    LOX_UNLOCK(db);
    return rc;
}

lox_err_t lox_ts_clear(lox_t *db, const char *name) {
    lox_core_t *core;
    lox_ts_stream_t *stream;
    lox_err_t rc = LOX_OK;
    uint32_t saved_head = 0u;
    uint32_t saved_tail = 0u;
    uint32_t saved_count = 0u;

    if (db == NULL) {
        return LOX_ERR_INVALID;
    }

    LOX_LOCK(db);
    core = lox_core(db);
    if (core->magic != LOX_MAGIC) {
        rc = LOX_ERR_INVALID;
        goto unlock;
    }

    stream = lox_ts_find(core, name);
    if (stream == NULL) {
        rc = LOX_ERR_NOT_FOUND;
        goto unlock;
    }

    if (core->wal_enabled && core->storage != NULL && !core->storage_loading && !core->wal_replaying) {
        rc = lox_persist_ts_clear(db, name);
        if (rc != LOX_OK) {
            goto unlock;
        }
        stream->head = 0u;
        stream->tail = 0u;
        stream->count = 0u;
        goto unlock;
    }

    saved_head = stream->head;
    saved_tail = stream->tail;
    saved_count = stream->count;
    stream->head = 0u;
    stream->tail = 0u;
    stream->count = 0u;
    rc = lox_storage_flush(db);
    if (rc != LOX_OK) {
        stream->head = saved_head;
        stream->tail = saved_tail;
        stream->count = saved_count;
    }

unlock:
    LOX_UNLOCK(db);
    return rc;
}
#else
lox_err_t lox_ts_register(lox_t *db, const char *name, lox_ts_type_t type, size_t raw_size) {
    (void)db;
    (void)name;
    (void)type;
    (void)raw_size;
    return LOX_ERR_DISABLED;
}

lox_err_t lox_ts_insert(lox_t *db, const char *name, lox_timestamp_t ts, const void *val) {
    (void)db;
    (void)name;
    (void)ts;
    (void)val;
    return LOX_ERR_DISABLED;
}

lox_err_t lox_ts_last(lox_t *db, const char *name, lox_ts_sample_t *out) {
    (void)db;
    (void)name;
    (void)out;
    return LOX_ERR_DISABLED;
}

lox_err_t lox_ts_query(lox_t *db,
                               const char *name,
                               lox_timestamp_t from,
                               lox_timestamp_t to,
                               lox_ts_query_cb_t cb,
                               void *ctx) {
    (void)db;
    (void)name;
    (void)from;
    (void)to;
    (void)cb;
    (void)ctx;
    return LOX_ERR_DISABLED;
}

lox_err_t lox_ts_query_buf(lox_t *db,
                                   const char *name,
                                   lox_timestamp_t from,
                                   lox_timestamp_t to,
                                   lox_ts_sample_t *buf,
                                   size_t max_count,
                                   size_t *out_count) {
    (void)db;
    (void)name;
    (void)from;
    (void)to;
    (void)buf;
    (void)max_count;
    (void)out_count;
    return LOX_ERR_DISABLED;
}

lox_err_t lox_ts_count(lox_t *db,
                               const char *name,
                               lox_timestamp_t from,
                               lox_timestamp_t to,
                               size_t *out_count) {
    (void)db;
    (void)name;
    (void)from;
    (void)to;
    (void)out_count;
    return LOX_ERR_DISABLED;
}

lox_err_t lox_ts_clear(lox_t *db, const char *name) {
    (void)db;
    (void)name;
    return LOX_ERR_DISABLED;
}
#endif
