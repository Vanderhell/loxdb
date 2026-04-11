#include "microtest.h"
#include "microdb.h"

#include <string.h>

static microdb_t g_db;
static uint32_t g_lock_count = 0u;
static uint32_t g_unlock_count = 0u;

static void *mock_lock_create(void) {
    return &g_lock_count;
}

static void mock_lock(void *hdl) {
    (void)hdl;
    g_lock_count++;
}

static void mock_unlock(void *hdl) {
    (void)hdl;
    g_unlock_count++;
}

static void mock_lock_destroy(void *hdl) {
    (void)hdl;
}

static void setup_db(void) {
    microdb_cfg_t cfg;

    memset(&g_db, 0, sizeof(g_db));
    memset(&cfg, 0, sizeof(cfg));
    g_lock_count = 0u;
    g_unlock_count = 0u;
    cfg.ram_kb = 32u;
    cfg.lock_create = mock_lock_create;
    cfg.lock = mock_lock;
    cfg.unlock = mock_unlock;
    cfg.lock_destroy = mock_lock_destroy;
    ASSERT_EQ(microdb_init(&g_db, &cfg), MICRODB_OK);
}

static void teardown_db(void) {
    (void)microdb_deinit(&g_db);
}

MDB_TEST(test_lock_called_on_kv_put) {
    uint8_t value = 1u;

    ASSERT_EQ(microdb_kv_put(&g_db, "a", &value, 1u), MICRODB_OK);
    ASSERT_EQ(g_lock_count, 1u);
    ASSERT_EQ(g_unlock_count, 1u);
}

MDB_TEST(test_lock_called_on_kv_get) {
    uint8_t value = 1u;
    uint8_t out = 0u;

    ASSERT_EQ(microdb_kv_put(&g_db, "a", &value, 1u), MICRODB_OK);
    g_lock_count = 0u;
    g_unlock_count = 0u;
    ASSERT_EQ(microdb_kv_get(&g_db, "a", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(g_lock_count, 1u);
    ASSERT_EQ(g_unlock_count, 1u);
}

MDB_TEST(test_counts_balanced_after_sequence) {
    uint8_t value = 1u;
    uint8_t out = 0u;
    uint32_t i;

    ASSERT_EQ(microdb_ts_register(&g_db, "s", MICRODB_TS_U32, 0u), MICRODB_OK);
    g_lock_count = 0u;
    g_unlock_count = 0u;

    for (i = 0u; i < 10u; ++i) {
        uint32_t tsv = i;
        ASSERT_EQ(microdb_kv_put(&g_db, "a", &value, 1u), MICRODB_OK);
        ASSERT_EQ(microdb_kv_get(&g_db, "a", &out, 1u, NULL), MICRODB_OK);
        ASSERT_EQ(microdb_ts_insert(&g_db, "s", i, &tsv), MICRODB_OK);
    }

    ASSERT_EQ(g_lock_count, g_unlock_count);
}

MDB_TEST(test_null_hooks_are_safe) {
    microdb_t db;
    microdb_cfg_t cfg;
    uint8_t value = 1u;

    memset(&db, 0, sizeof(db));
    memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 32u;
    cfg.lock_create = mock_lock_create;
    cfg.lock = NULL;
    cfg.unlock = NULL;
    cfg.lock_destroy = mock_lock_destroy;
    ASSERT_EQ(microdb_init(&db, &cfg), MICRODB_OK);
    ASSERT_EQ(microdb_kv_put(&db, "n", &value, 1u), MICRODB_OK);
    ASSERT_EQ(microdb_deinit(&db), MICRODB_OK);
}

int main(void) {
    MDB_RUN_TEST(setup_db, teardown_db, test_lock_called_on_kv_put);
    MDB_RUN_TEST(setup_db, teardown_db, test_lock_called_on_kv_get);
    MDB_RUN_TEST(setup_db, teardown_db, test_counts_balanced_after_sequence);
    MDB_RUN_TEST(setup_db, teardown_db, test_null_hooks_are_safe);
    return MDB_RESULT();
}
