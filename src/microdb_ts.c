#include "microdb_internal.h"

static microdb_err_t microdb_ts_unavailable(microdb_t *db) {
    if (db == NULL) {
        return MICRODB_ERR_INVALID;
    }
    return MICRODB_ERR_DISABLED;
}

#if MICRODB_ENABLE_TS
microdb_err_t microdb_ts_register(microdb_t *db, const char *name, microdb_ts_type_t type, size_t raw_size) {
    (void)name;
    (void)type;
    (void)raw_size;
    return microdb_ts_unavailable(db);
}

microdb_err_t microdb_ts_insert(microdb_t *db, const char *name, microdb_timestamp_t ts, const void *val) {
    (void)name;
    (void)ts;
    (void)val;
    return microdb_ts_unavailable(db);
}

microdb_err_t microdb_ts_last(microdb_t *db, const char *name, microdb_ts_sample_t *out) {
    (void)name;
    (void)out;
    return microdb_ts_unavailable(db);
}

microdb_err_t microdb_ts_query(microdb_t *db, const char *name, microdb_timestamp_t from, microdb_timestamp_t to, microdb_ts_query_cb_t cb, void *ctx) {
    (void)name;
    (void)from;
    (void)to;
    (void)cb;
    (void)ctx;
    return microdb_ts_unavailable(db);
}

microdb_err_t microdb_ts_query_buf(microdb_t *db, const char *name, microdb_timestamp_t from, microdb_timestamp_t to, microdb_ts_sample_t *buf, size_t max_count, size_t *out_count) {
    (void)name;
    (void)from;
    (void)to;
    (void)buf;
    (void)max_count;
    (void)out_count;
    return microdb_ts_unavailable(db);
}

microdb_err_t microdb_ts_count(microdb_t *db, const char *name, microdb_timestamp_t from, microdb_timestamp_t to, size_t *out_count) {
    (void)name;
    (void)from;
    (void)to;
    (void)out_count;
    return microdb_ts_unavailable(db);
}

microdb_err_t microdb_ts_clear(microdb_t *db, const char *name) {
    (void)name;
    return microdb_ts_unavailable(db);
}
#endif
