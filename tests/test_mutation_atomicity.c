// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    FAIL_NONE = 0,
    FAIL_WRITE,
    FAIL_ERASE,
    FAIL_SYNC
} fail_kind_t;

typedef struct {
    uint8_t bytes[262144];
    uint32_t writes;
    uint32_t erases;
    uint32_t syncs;
    uint32_t fail_call;
    fail_kind_t fail_kind;
    bool partial_write;
} atomic_media_t;

static atomic_media_t g_media;
static lox_storage_t g_storage;
static lox_t g_db;

static lox_err_t media_read(void *ctx, uint32_t offset, void *buf, size_t len) {
    atomic_media_t *media = (atomic_media_t *)ctx;
    if (media == NULL || buf == NULL || offset > sizeof(media->bytes) ||
        len > sizeof(media->bytes) - offset) {
        return LOX_ERR_INVALID;
    }
    memcpy(buf, media->bytes + offset, len);
    return LOX_OK;
}

static lox_err_t media_write(void *ctx, uint32_t offset, const void *buf, size_t len) {
    atomic_media_t *media = (atomic_media_t *)ctx;
    if (media == NULL || buf == NULL || offset > sizeof(media->bytes) ||
        len > sizeof(media->bytes) - offset) {
        return LOX_ERR_INVALID;
    }
    media->writes++;
    if (media->fail_kind == FAIL_WRITE && media->writes == media->fail_call) {
        if (media->partial_write && len != 0u) {
            size_t partial = len / 2u;
            if (partial == 0u) {
                partial = 1u;
            }
            memcpy(media->bytes + offset, buf, partial);
        }
        return LOX_ERR_STORAGE;
    }
    memcpy(media->bytes + offset, buf, len);
    return LOX_OK;
}

static lox_err_t media_erase(void *ctx, uint32_t offset) {
    atomic_media_t *media = (atomic_media_t *)ctx;
    if (media == NULL || offset > sizeof(media->bytes) ||
        4096u > sizeof(media->bytes) - offset) {
        return LOX_ERR_INVALID;
    }
    media->erases++;
    if (media->fail_kind == FAIL_ERASE && media->erases == media->fail_call) {
        memset(media->bytes + offset, 0xFF, 2048u);
        return LOX_ERR_STORAGE;
    }
    memset(media->bytes + offset, 0xFF, 4096u);
    return LOX_OK;
}

static lox_err_t media_sync(void *ctx) {
    atomic_media_t *media = (atomic_media_t *)ctx;
    if (media == NULL) {
        return LOX_ERR_INVALID;
    }
    media->syncs++;
    if (media->fail_kind == FAIL_SYNC && media->syncs == media->fail_call) {
        return LOX_ERR_STORAGE;
    }
    return LOX_OK;
}

static void reset_trace(void) {
    g_media.writes = 0u;
    g_media.erases = 0u;
    g_media.syncs = 0u;
    g_media.fail_call = 0u;
    g_media.fail_kind = FAIL_NONE;
    g_media.partial_write = false;
}

static void init_handle(lox_t *db) {
    lox_cfg_t cfg;
    memset(db, 0, sizeof(*db));
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    cfg.wal_sync_mode = LOX_WAL_SYNC_ALWAYS;
    ASSERT_EQ(lox_init(db, &cfg), LOX_OK);
}

static void setup_db(void) {
    memset(&g_media, 0, sizeof(g_media));
    memset(g_media.bytes, 0xFF, sizeof(g_media.bytes));
    memset(&g_storage, 0, sizeof(g_storage));
    g_storage.read = media_read;
    g_storage.write = media_write;
    g_storage.erase = media_erase;
    g_storage.sync = media_sync;
    g_storage.capacity = sizeof(g_media.bytes);
    g_storage.erase_size = 4096u;
    g_storage.write_size = 1u;
    g_storage.ctx = &g_media;
    init_handle(&g_db);
    reset_trace();
}

static void teardown_db(void) {
    reset_trace();
    (void)lox_deinit(&g_db);
}

static void make_table(lox_t *db, const char *name, uint32_t max_rows, lox_table_t **out) {
    lox_schema_t schema;
    ASSERT_EQ(lox_schema_init(&schema, name, max_rows), LOX_OK);
    ASSERT_EQ(lox_schema_add(&schema, "id", LOX_COL_U32, sizeof(uint32_t), true), LOX_OK);
    ASSERT_EQ(lox_schema_add(&schema, "tag", LOX_COL_STR, 4u, false), LOX_OK);
    ASSERT_EQ(lox_schema_seal(&schema), LOX_OK);
    ASSERT_EQ(lox_table_create(db, &schema), LOX_OK);
    ASSERT_EQ(lox_table_get(db, name, out), LOX_OK);
}

MDB_TEST(deterministic_failures_do_not_touch_backend) {
    uint8_t value[LOX_KV_VAL_MAX_LEN + 1u] = {0};
    uint32_t before;
    lox_schema_t unsealed;
    lox_table_t *table = NULL;
    uint8_t row[64];
    uint32_t id = 1u;

    before = g_media.writes + g_media.erases + g_media.syncs;
    ASSERT_EQ(lox_kv_del(&g_db, "missing"), LOX_ERR_NOT_FOUND);
    ASSERT_EQ(lox_kv_put(&g_db, "too-big", value, sizeof(value)), LOX_ERR_OVERFLOW);
    ASSERT_EQ(lox_ts_insert(&g_db, "missing", 1u, &id), LOX_ERR_NOT_FOUND);
    ASSERT_EQ(lox_ts_register(&g_db, "bad", LOX_TS_RAW, 0u), LOX_ERR_INVALID);
    ASSERT_EQ(lox_schema_init(&unsealed, "nope", 1u), LOX_OK);
    ASSERT_EQ(lox_table_create(&g_db, &unsealed), LOX_ERR_INVALID);
    ASSERT_EQ(g_media.writes + g_media.erases + g_media.syncs, before);

    make_table(&g_db, "rows", 1u, &table);
    memset(row, 'x', sizeof(row));
    ASSERT_EQ(lox_row_set(table, row, "id", &id, sizeof(id)), LOX_OK);
    reset_trace();
    ASSERT_EQ(lox_rel_insert(&g_db, table, row), LOX_ERR_SCHEMA);
    ASSERT_EQ(g_media.writes + g_media.erases + g_media.syncs, 0u);
}

MDB_TEST(partial_kv_write_faults_handle_and_reopen_resolves) {
    lox_t reopened;
    uint8_t value = 7u;
    uint8_t out = 0u;
    lox_stats_t stats;

    g_media.fail_kind = FAIL_WRITE;
    g_media.fail_call = 1u;
    g_media.partial_write = true;
    ASSERT_EQ(lox_kv_put(&g_db, "partial", &value, 1u), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_kv_get(&g_db, "partial", &out, 1u, NULL), LOX_ERR_NOT_FOUND);
    ASSERT_EQ(lox_stats(&g_db, &stats), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "blocked", &value, 1u), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_flush(&g_db), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_compact(&g_db), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_deinit(&g_db), LOX_ERR_INDETERMINATE);

    reset_trace();
    init_handle(&reopened);
    ASSERT_EQ(lox_kv_get(&reopened, "partial", &out, 1u, NULL), LOX_ERR_NOT_FOUND);
    ASSERT_EQ(lox_deinit(&reopened), LOX_OK);
    memset(&g_db, 0, sizeof(g_db));
}

MDB_TEST(sync_failure_can_replay_completed_kv_record) {
    lox_t reopened;
    uint8_t value = 9u;
    uint8_t out = 0u;

    g_media.fail_kind = FAIL_SYNC;
    g_media.fail_call = 1u;
    ASSERT_EQ(lox_kv_put(&g_db, "synced", &value, 1u), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_deinit(&g_db), LOX_ERR_INDETERMINATE);
    reset_trace();
    init_handle(&reopened);
    ASSERT_EQ(lox_kv_get(&reopened, "synced", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, value);
    ASSERT_EQ(lox_deinit(&reopened), LOX_OK);
    memset(&g_db, 0, sizeof(g_db));
}

MDB_TEST(ts_rel_and_transaction_failures_are_indeterminate) {
    lox_table_t *table = NULL;
    uint8_t row[64] = {0};
    uint32_t id = 1u;
    uint32_t sample = 42u;
    uint8_t value = 3u;

    ASSERT_EQ(lox_ts_register(&g_db, "s", LOX_TS_U32, 0u), LOX_OK);
    make_table(&g_db, "r", 4u, &table);
    ASSERT_EQ(lox_row_set(table, row, "id", &id, sizeof(id)), LOX_OK);
    ASSERT_EQ(lox_row_set(table, row, "tag", "ok", sizeof("ok") - 1u), LOX_OK);
    reset_trace();
    g_media.fail_kind = FAIL_WRITE;
    g_media.fail_call = 1u;
    ASSERT_EQ(lox_ts_insert(&g_db, "s", 1u, &sample), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_deinit(&g_db), LOX_ERR_INDETERMINATE);

    reset_trace();
    init_handle(&g_db);
    ASSERT_EQ(lox_table_get(&g_db, "r", &table), LOX_OK);
    reset_trace();
    g_media.fail_kind = FAIL_WRITE;
    g_media.fail_call = 1u;
    ASSERT_EQ(lox_rel_insert(&g_db, table, row), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_deinit(&g_db), LOX_ERR_INDETERMINATE);

    reset_trace();
    init_handle(&g_db);
    ASSERT_EQ(lox_txn_begin(&g_db), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "tx", &value, 1u), LOX_OK);
    reset_trace();
    g_media.fail_kind = FAIL_WRITE;
    g_media.fail_call = 1u;
    ASSERT_EQ(lox_txn_commit(&g_db), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_deinit(&g_db), LOX_ERR_INDETERMINATE);
}

MDB_TEST(compaction_erase_failure_faults_handle) {
    uint8_t value = 1u;
    ASSERT_EQ(lox_kv_put(&g_db, "stable", &value, 1u), LOX_OK);
    reset_trace();
    g_media.fail_kind = FAIL_ERASE;
    g_media.fail_call = 1u;
    ASSERT_EQ(lox_compact(&g_db), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(lox_kv_get(&g_db, "stable", &value, 1u, NULL), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "blocked", &value, 1u), LOX_ERR_INDETERMINATE);
}

MDB_TEST(transaction_final_state_is_admitted_before_wal) {
    lox_effective_capacity_t capacity;
    uint8_t value[LOX_KV_VAL_MAX_LEN];
    char key[16];
    uint32_t index = 0u;

    memset(value, 0xA5, sizeof(value));
    ASSERT_EQ(lox_get_effective_capacity(&g_db, &capacity), LOX_OK);
    while (capacity.kv_value_bytes_free_now >= sizeof(value)) {
        (void)snprintf(key, sizeof(key), "fill%u", (unsigned)index++);
        ASSERT_EQ(lox_kv_put(&g_db, key, value, sizeof(value)), LOX_OK);
        ASSERT_EQ(lox_get_effective_capacity(&g_db, &capacity), LOX_OK);
    }
    if (capacity.kv_value_bytes_free_now != 0u) {
        (void)snprintf(key, sizeof(key), "fill%u", (unsigned)index++);
        ASSERT_EQ(lox_kv_put(&g_db, key, value, capacity.kv_value_bytes_free_now), LOX_OK);
    }

    ASSERT_EQ(lox_txn_begin(&g_db), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "cannot-fit", value, 1u), LOX_OK);
    reset_trace();
    ASSERT_EQ(lox_txn_commit(&g_db), LOX_ERR_NO_MEM);
    ASSERT_EQ(g_media.writes + g_media.erases + g_media.syncs, 0u);
    ASSERT_EQ(lox_txn_rollback(&g_db), LOX_OK);
}

MDB_TEST(transaction_duplicate_keys_commit_final_value) {
    lox_t reopened;
    uint8_t a = 1u;
    uint8_t b = 2u;
    uint8_t c = 3u;
    uint8_t out = 0u;

    ASSERT_EQ(lox_txn_begin(&g_db), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "dup", &a, 1u), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "dup", &b, 1u), LOX_OK);
    ASSERT_EQ(lox_kv_del(&g_db, "dup"), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "dup", &c, 1u), LOX_OK);
    ASSERT_EQ(lox_txn_commit(&g_db), LOX_OK);
    ASSERT_EQ(lox_kv_get(&g_db, "dup", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, c);

    init_handle(&reopened);
    ASSERT_EQ(lox_kv_get(&reopened, "dup", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, c);
    ASSERT_EQ(lox_deinit(&reopened), LOX_OK);
}

MDB_TEST(sync_always_success_survives_immediate_reopen) {
    lox_t reopened;
    uint8_t value = 0x5Au;
    uint8_t out = 0u;
    ASSERT_EQ(lox_kv_put(&g_db, "durable", &value, 1u), LOX_OK);
    init_handle(&reopened);
    ASSERT_EQ(lox_kv_get(&reopened, "durable", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, value);
    ASSERT_EQ(lox_deinit(&reopened), LOX_OK);
}

int main(void) {
    MDB_RUN_TEST(setup_db, teardown_db, deterministic_failures_do_not_touch_backend);
    MDB_RUN_TEST(setup_db, teardown_db, partial_kv_write_faults_handle_and_reopen_resolves);
    MDB_RUN_TEST(setup_db, teardown_db, sync_failure_can_replay_completed_kv_record);
    MDB_RUN_TEST(setup_db, teardown_db, ts_rel_and_transaction_failures_are_indeterminate);
    MDB_RUN_TEST(setup_db, teardown_db, compaction_erase_failure_faults_handle);
    MDB_RUN_TEST(setup_db, teardown_db, transaction_final_state_is_admitted_before_wal);
    MDB_RUN_TEST(setup_db, teardown_db, transaction_duplicate_keys_commit_final_value);
    MDB_RUN_TEST(setup_db, teardown_db, sync_always_success_survives_immediate_reopen);
    return MDB_RESULT();
}
