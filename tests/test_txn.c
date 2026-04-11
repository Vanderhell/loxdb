#include "microtest.h"
#include "microdb.h"
#include "../port/posix/microdb_port_posix.h"

#include <string.h>

static microdb_t g_db;
static microdb_storage_t g_storage;
static const char *g_path = "txn_test.bin";

static void open_db(void) {
    microdb_cfg_t cfg;

    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    ASSERT_EQ(microdb_port_posix_init(&g_storage, g_path, 65536u), MICRODB_OK);
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    ASSERT_EQ(microdb_init(&g_db, &cfg), MICRODB_OK);
}

static void setup_db(void) {
    microdb_port_posix_remove(g_path);
    open_db();
}

static void teardown_db(void) {
    (void)microdb_deinit(&g_db);
    microdb_port_posix_deinit(&g_storage);
    microdb_port_posix_remove(g_path);
}

static void reopen_db(void) {
    ASSERT_EQ(microdb_deinit(&g_db), MICRODB_OK);
    microdb_port_posix_deinit(&g_storage);
    open_db();
}

MDB_TEST(test_txn_commit_makes_values_visible) {
    uint8_t out = 0u;

    ASSERT_EQ(microdb_txn_begin(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_put(&g_db, "A", &(uint8_t){ 1u }, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_kv_put(&g_db, "B", &(uint8_t){ 2u }, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_txn_commit(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 1u);
    ASSERT_EQ(microdb_kv_get(&g_db, "B", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 2u);
}

MDB_TEST(test_txn_rollback_discards_values) {
    uint8_t out = 0u;

    ASSERT_EQ(microdb_txn_begin(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_put(&g_db, "A", &(uint8_t){ 1u }, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_txn_rollback(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_ERR_NOT_FOUND);
}

MDB_TEST(test_txn_double_begin_returns_err) {
    ASSERT_EQ(microdb_txn_begin(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_txn_begin(&g_db), MICRODB_ERR_TXN_ACTIVE);
}

MDB_TEST(test_txn_get_sees_staged_value) {
    uint8_t out = 0u;
    uint8_t v1 = 1u;
    uint8_t v2 = 2u;

    ASSERT_EQ(microdb_kv_put(&g_db, "A", &v1, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_txn_begin(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_put(&g_db, "A", &v2, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 2u);
    ASSERT_EQ(microdb_txn_rollback(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 1u);
}

MDB_TEST(test_txn_del_inside_txn) {
    uint8_t out = 0u;
    uint8_t v1 = 1u;

    ASSERT_EQ(microdb_kv_put(&g_db, "A", &v1, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_txn_begin(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_del(&g_db, "A"), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_ERR_NOT_FOUND);
    ASSERT_EQ(microdb_txn_rollback(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 1u);
}

MDB_TEST(test_kv_put_outside_txn_unaffected) {
    uint8_t out = 0u;

    ASSERT_EQ(microdb_txn_begin(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_put(&g_db, "A", &(uint8_t){ 1u }, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_txn_commit(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_put(&g_db, "B", &(uint8_t){ 5u }, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "B", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 5u);
}

MDB_TEST(test_txn_no_commit_survives_reinit) {
    uint8_t out = 0u;

    ASSERT_EQ(microdb_txn_begin(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_put(&g_db, "A", &(uint8_t){ 1u }, 1u), MICRODB_OK);
    reopen_db();
    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_ERR_NOT_FOUND);
}

int main(void) {
    MDB_RUN_TEST(setup_db, teardown_db, test_txn_commit_makes_values_visible);
    MDB_RUN_TEST(setup_db, teardown_db, test_txn_rollback_discards_values);
    MDB_RUN_TEST(setup_db, teardown_db, test_txn_double_begin_returns_err);
    MDB_RUN_TEST(setup_db, teardown_db, test_txn_get_sees_staged_value);
    MDB_RUN_TEST(setup_db, teardown_db, test_txn_del_inside_txn);
    MDB_RUN_TEST(setup_db, teardown_db, test_kv_put_outside_txn_unaffected);
    MDB_RUN_TEST(setup_db, teardown_db, test_txn_no_commit_survives_reinit);
    return MDB_RESULT();
}
