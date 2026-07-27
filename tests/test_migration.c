// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "../port/posix/lox_port_posix.h"
#include "../src/lox_internal.h"
#include "strict_nor_emulator.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    SCHEMA_IDENTICAL = 0,
    SCHEMA_EXTRA_COLUMN,
    SCHEMA_RENAMED_COLUMN,
    SCHEMA_CHANGED_TYPE,
    SCHEMA_CHANGED_SIZE,
    SCHEMA_CHANGED_INDEX,
    SCHEMA_CHANGED_MAX_ROWS
} schema_variant_t;

static lox_t g_db;
static lox_storage_t g_storage;
static const char *g_path = "migration_test.bin";
static uint32_t g_migrate_calls;
static uint16_t g_migrate_old;
static uint16_t g_migrate_new;
static lox_err_t g_migrate_result;
static uint32_t g_nested_migrate_calls;
static lox_err_t g_nested_migrate_result;
static uint32_t g_lock_depth;
static uint32_t g_callback_lock_depth;

static lox_err_t create_users_table(lox_t *db, uint16_t schema_version, schema_variant_t variant);

static void *mock_lock_create(void) {
    return &g_lock_depth;
}

static void mock_lock(void *handle) {
    (void)handle;
    g_lock_depth++;
}

static void mock_unlock(void *handle) {
    (void)handle;
    if (g_lock_depth != 0u) {
        g_lock_depth--;
    }
}

static void mock_lock_destroy(void *handle) {
    (void)handle;
}

static lox_err_t on_migrate_cb(lox_t *db,
                               const char *table_name,
                               uint16_t old_version,
                               uint16_t new_version) {
    (void)db;
    (void)table_name;
    g_callback_lock_depth = g_lock_depth;
    g_migrate_calls++;
    g_migrate_old = old_version;
    g_migrate_new = new_version;
    return g_migrate_result;
}

static lox_err_t on_migrate_recursive_cb(lox_t *db,
                                         const char *table_name,
                                         uint16_t old_version,
                                         uint16_t new_version) {
    (void)table_name;
    g_callback_lock_depth = g_lock_depth;
    g_migrate_calls++;
    g_migrate_old = old_version;
    g_migrate_new = new_version;
    g_nested_migrate_calls++;
    g_nested_migrate_result =
        create_users_table(db, (uint16_t)(new_version + 1u), SCHEMA_IDENTICAL);
    return g_nested_migrate_result;
}

static void reset_callback_state(void) {
    g_migrate_calls = 0u;
    g_migrate_old = 0u;
    g_migrate_new = 0u;
    g_migrate_result = LOX_OK;
    g_nested_migrate_calls = 0u;
    g_nested_migrate_result = LOX_OK;
    g_callback_lock_depth = UINT32_MAX;
}

static void fill_cfg(lox_cfg_t *cfg,
                     lox_storage_t *storage,
                     lox_err_t (*on_migrate)(lox_t *, const char *, uint16_t, uint16_t)) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->storage = storage;
    cfg->ram_kb = 32u;
    cfg->wal_sync_mode = LOX_WAL_SYNC_ALWAYS;
    cfg->on_migrate = on_migrate;
    cfg->lock_create = mock_lock_create;
    cfg->lock = mock_lock;
    cfg->unlock = mock_unlock;
    cfg->lock_destroy = mock_lock_destroy;
}

static lox_err_t open_db_result(
    lox_err_t (*on_migrate)(lox_t *, const char *, uint16_t, uint16_t)) {
    lox_cfg_t cfg;

    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    if (lox_port_posix_init(&g_storage, g_path, 131072u) != LOX_OK) {
        return LOX_ERR_STORAGE;
    }
    fill_cfg(&cfg, &g_storage, on_migrate);
    return lox_init(&g_db, &cfg);
}

static void open_db(
    lox_err_t (*on_migrate)(lox_t *, const char *, uint16_t, uint16_t)) {
    ASSERT_EQ(open_db_result(on_migrate), LOX_OK);
}

static void reopen_db(
    lox_err_t (*on_migrate)(lox_t *, const char *, uint16_t, uint16_t)) {
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
    lox_port_posix_deinit(&g_storage);
    open_db(on_migrate);
}

static void crash_close_db(void) {
    lox_port_posix_simulate_power_loss(&g_storage);
    free(lox_core(&g_db)->heap);
    memset(&g_db, 0, sizeof(g_db));
    lox_port_posix_deinit(&g_storage);
    memset(&g_storage, 0, sizeof(g_storage));
}

static void setup_db(void) {
    lox_port_posix_remove(g_path);
    g_lock_depth = 0u;
    reset_callback_state();
    open_db(NULL);
}

static void teardown_db(void) {
    if (lox_core_const(&g_db)->magic == LOX_MAGIC) {
        (void)lox_deinit(&g_db);
    }
    lox_port_posix_deinit(&g_storage);
    lox_port_posix_remove(g_path);
}

static lox_err_t build_users_schema(lox_schema_t *schema,
                                    uint16_t schema_version,
                                    schema_variant_t variant) {
    lox_err_t err;
    uint32_t max_rows = variant == SCHEMA_CHANGED_MAX_ROWS ? 9u : 8u;
    const char *second_name = variant == SCHEMA_RENAMED_COLUMN ? "alias" : "name";
    lox_col_type_t second_type =
        variant == SCHEMA_CHANGED_TYPE ? LOX_COL_BLOB : LOX_COL_STR;
    size_t second_size = variant == SCHEMA_CHANGED_SIZE ? 9u : 8u;
    bool id_index = variant != SCHEMA_CHANGED_INDEX;
    bool second_index = variant == SCHEMA_CHANGED_INDEX;

    err = lox_schema_init(schema, "users", max_rows);
    if (err != LOX_OK) {
        return err;
    }
    schema->schema_version = schema_version;
    err = lox_schema_add(schema, "id", LOX_COL_U32, sizeof(uint32_t), id_index);
    if (err != LOX_OK) {
        return err;
    }
    err = lox_schema_add(schema, second_name, second_type, second_size, second_index);
    if (err != LOX_OK) {
        return err;
    }
    if (variant == SCHEMA_EXTRA_COLUMN) {
        err = lox_schema_add(schema, "active", LOX_COL_U8, sizeof(uint8_t), false);
        if (err != LOX_OK) {
            return err;
        }
    }
    return lox_schema_seal(schema);
}

static lox_err_t create_users_table(lox_t *db,
                                    uint16_t schema_version,
                                    schema_variant_t variant) {
    lox_schema_t schema;
    lox_err_t err = build_users_schema(&schema, schema_version, variant);
    return err == LOX_OK ? lox_table_create(db, &schema) : err;
}

static uint16_t users_version(lox_t *db) {
    lox_table_t *table = NULL;
    if (lox_table_get(db, "users", &table) != LOX_OK || table == NULL) {
        return UINT16_MAX;
    }
    return table->schema_version;
}

static void assert_same_version_rejected(schema_variant_t variant) {
    uint32_t wal_before;

    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_cb);
    wal_before = lox_core_const(&g_db)->wal_used;
    ASSERT_EQ(create_users_table(&g_db, 1u, variant), LOX_ERR_SCHEMA);
    ASSERT_EQ(g_migrate_calls, 0u);
    ASSERT_EQ(lox_core_const(&g_db)->wal_used, wal_before);
    ASSERT_EQ(users_version(&g_db), 1u);
}

MDB_TEST(same_version_identical_schema_is_idempotent) {
    uint32_t wal_before;

    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_cb);
    wal_before = lox_core_const(&g_db)->wal_used;
    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    ASSERT_EQ(g_migrate_calls, 0u);
    ASSERT_EQ(lox_core_const(&g_db)->wal_used, wal_before);
    ASSERT_EQ(users_version(&g_db), 1u);
}

MDB_TEST(same_version_changed_column_count_is_schema_error) {
    assert_same_version_rejected(SCHEMA_EXTRA_COLUMN);
}

MDB_TEST(same_version_renamed_column_is_schema_error) {
    assert_same_version_rejected(SCHEMA_RENAMED_COLUMN);
}

MDB_TEST(same_version_changed_type_is_schema_error) {
    assert_same_version_rejected(SCHEMA_CHANGED_TYPE);
}

MDB_TEST(same_version_changed_size_is_schema_error) {
    assert_same_version_rejected(SCHEMA_CHANGED_SIZE);
}

MDB_TEST(same_version_changed_index_is_schema_error) {
    assert_same_version_rejected(SCHEMA_CHANGED_INDEX);
}

MDB_TEST(same_version_changed_max_rows_is_schema_error) {
    assert_same_version_rejected(SCHEMA_CHANGED_MAX_ROWS);
}

MDB_TEST(version_transition_identical_schema_calls_outside_lock) {
    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_cb);
    ASSERT_EQ(create_users_table(&g_db, 2u, SCHEMA_IDENTICAL), LOX_OK);
    ASSERT_EQ(g_migrate_calls, 1u);
    ASSERT_EQ(g_migrate_old, 1u);
    ASSERT_EQ(g_migrate_new, 2u);
    ASSERT_EQ(g_callback_lock_depth, 0u);
    ASSERT_EQ(users_version(&g_db), 2u);
}

MDB_TEST(version_transition_without_callback_is_schema_error) {
    uint32_t wal_before;

    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(NULL);
    wal_before = lox_core_const(&g_db)->wal_used;
    ASSERT_EQ(create_users_table(&g_db, 2u, SCHEMA_IDENTICAL), LOX_ERR_SCHEMA);
    ASSERT_EQ(lox_core_const(&g_db)->wal_used, wal_before);
    ASSERT_EQ(users_version(&g_db), 1u);
}

MDB_TEST(version_transition_incompatible_schema_skips_callback_and_wal) {
    uint32_t wal_before;

    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_cb);
    wal_before = lox_core_const(&g_db)->wal_used;
    ASSERT_EQ(create_users_table(&g_db, 2u, SCHEMA_RENAMED_COLUMN), LOX_ERR_SCHEMA);
    ASSERT_EQ(g_migrate_calls, 0u);
    ASSERT_EQ(lox_core_const(&g_db)->wal_used, wal_before);
    ASSERT_EQ(users_version(&g_db), 1u);
}

MDB_TEST(callback_failure_preserves_old_version) {
    uint32_t wal_before;

    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_cb);
    g_migrate_result = LOX_ERR_INVALID;
    wal_before = lox_core_const(&g_db)->wal_used;
    ASSERT_EQ(create_users_table(&g_db, 2u, SCHEMA_IDENTICAL), LOX_ERR_INVALID);
    ASSERT_EQ(g_migrate_calls, 1u);
    ASSERT_EQ(lox_core_const(&g_db)->wal_used, wal_before);
    ASSERT_EQ(users_version(&g_db), 1u);
}

MDB_TEST(recursive_callback_attempt_is_rejected) {
    uint32_t wal_before;

    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_recursive_cb);
    wal_before = lox_core_const(&g_db)->wal_used;
    ASSERT_EQ(create_users_table(&g_db, 2u, SCHEMA_IDENTICAL), LOX_ERR_SCHEMA);
    ASSERT_EQ(g_migrate_calls, 1u);
    ASSERT_EQ(g_nested_migrate_calls, 1u);
    ASSERT_EQ(g_nested_migrate_result, LOX_ERR_SCHEMA);
    ASSERT_EQ(g_callback_lock_depth, 0u);
    ASSERT_EQ(lox_core_const(&g_db)->wal_used, wal_before);
    ASSERT_EQ(users_version(&g_db), 1u);
}

MDB_TEST(deterministic_persistence_failure_preserves_old_version) {
    lox_core_t *core;
    uint32_t saved_wal_size;

    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_cb);
    core = lox_core(&g_db);
    saved_wal_size = core->layout.wal_size;
    core->layout.wal_size = lox_wal_header_bytes(core) + 1u;
    ASSERT_EQ(create_users_table(&g_db, 2u, SCHEMA_IDENTICAL), LOX_ERR_FULL);
    core->layout.wal_size = saved_wal_size;
    ASSERT_EQ(g_migrate_calls, 1u);
    ASSERT_EQ(users_version(&g_db), 1u);
    ASSERT_EQ(core->storage_faulted, false);
}

MDB_TEST(indeterminate_persistence_preserves_old_version) {
    static nor_flash_ctx_t media;
    lox_storage_t storage;
    lox_cfg_t cfg;
    lox_t db;

    memset(&media, 0, sizeof(media));
    nor_flash_reset(&media);
    nor_flash_bind_storage(&storage, &media, 4096u, 1u);
    memset(&db, 0, sizeof(db));
    fill_cfg(&cfg, &storage, on_migrate_cb);
    ASSERT_EQ(lox_init(&db, &cfg), LOX_OK);
    ASSERT_EQ(create_users_table(&db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reset_callback_state();
    media.fail_next_sync = 1u;
    ASSERT_EQ(create_users_table(&db, 2u, SCHEMA_IDENTICAL), LOX_ERR_INDETERMINATE);
    ASSERT_EQ(g_migrate_calls, 1u);
    ASSERT_EQ(users_version(&db), 1u);
    ASSERT_EQ(lox_core_const(&db)->storage_faulted, true);
    ASSERT_EQ(lox_deinit(&db), LOX_ERR_INDETERMINATE);

    nor_flash_power_loss(&media);
    nor_flash_bind_storage(&storage, &media, 4096u, 1u);
    memset(&db, 0, sizeof(db));
    fill_cfg(&cfg, &storage, NULL);
    ASSERT_EQ(lox_init(&db, &cfg), LOX_OK);
    ASSERT_EQ(users_version(&db), 1u);
    ASSERT_EQ(lox_deinit(&db), LOX_OK);
}

MDB_TEST(close_reopen_preserves_successful_transition) {
    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_cb);
    ASSERT_EQ(create_users_table(&g_db, 2u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(NULL);
    ASSERT_EQ(users_version(&g_db), 2u);
}

MDB_TEST(wal_replay_applies_compatible_transition_without_callback) {
    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    reopen_db(on_migrate_cb);
    ASSERT_EQ(create_users_table(&g_db, 2u, SCHEMA_IDENTICAL), LOX_OK);
    crash_close_db();
    open_db(NULL);
    ASSERT_EQ(users_version(&g_db), 2u);
}

MDB_TEST(wal_replay_rejects_incompatible_table_definition) {
    lox_schema_t incompatible;

    ASSERT_EQ(create_users_table(&g_db, 1u, SCHEMA_IDENTICAL), LOX_OK);
    ASSERT_EQ(build_users_schema(&incompatible, 2u, SCHEMA_RENAMED_COLUMN), LOX_OK);
    ASSERT_EQ(lox_persist_rel_table_create(&g_db, &incompatible), LOX_OK);
    crash_close_db();
    ASSERT_EQ(open_db_result(NULL), LOX_ERR_SCHEMA);
}

int main(void) {
    MDB_RUN_TEST(setup_db, teardown_db, same_version_identical_schema_is_idempotent);
    MDB_RUN_TEST(setup_db, teardown_db, same_version_changed_column_count_is_schema_error);
    MDB_RUN_TEST(setup_db, teardown_db, same_version_renamed_column_is_schema_error);
    MDB_RUN_TEST(setup_db, teardown_db, same_version_changed_type_is_schema_error);
    MDB_RUN_TEST(setup_db, teardown_db, same_version_changed_size_is_schema_error);
    MDB_RUN_TEST(setup_db, teardown_db, same_version_changed_index_is_schema_error);
    MDB_RUN_TEST(setup_db, teardown_db, same_version_changed_max_rows_is_schema_error);
    MDB_RUN_TEST(setup_db, teardown_db, version_transition_identical_schema_calls_outside_lock);
    MDB_RUN_TEST(setup_db, teardown_db, version_transition_without_callback_is_schema_error);
    MDB_RUN_TEST(setup_db, teardown_db, version_transition_incompatible_schema_skips_callback_and_wal);
    MDB_RUN_TEST(setup_db, teardown_db, callback_failure_preserves_old_version);
    MDB_RUN_TEST(setup_db, teardown_db, recursive_callback_attempt_is_rejected);
    MDB_RUN_TEST(setup_db, teardown_db, deterministic_persistence_failure_preserves_old_version);
    MDB_RUN_TEST(setup_db, teardown_db, indeterminate_persistence_preserves_old_version);
    MDB_RUN_TEST(setup_db, teardown_db, close_reopen_preserves_successful_transition);
    MDB_RUN_TEST(setup_db, teardown_db, wal_replay_applies_compatible_transition_without_callback);
    MDB_RUN_TEST(setup_db, teardown_db, wal_replay_rejects_incompatible_table_definition);
    return MDB_RESULT();
}
