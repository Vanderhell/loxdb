#include "microtest.h"
#include "microdb.h"
#include "../port/posix/microdb_port_posix.h"

#include <string.h>

static microdb_t g_db;
static microdb_storage_t g_storage;
static const char *g_path = "stats_test.bin";

static void setup_db(void) {
    microdb_cfg_t cfg;

    memset(&g_db, 0, sizeof(g_db));
    memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 32u;
    ASSERT_EQ(microdb_init(&g_db, &cfg), MICRODB_OK);
}

static void teardown_db(void) {
    (void)microdb_deinit(&g_db);
}

static void setup_storage_db(void) {
    microdb_cfg_t cfg;

    microdb_port_posix_remove(g_path);
    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    ASSERT_EQ(microdb_port_posix_init(&g_storage, g_path, 65536u), MICRODB_OK);
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    ASSERT_EQ(microdb_init(&g_db, &cfg), MICRODB_OK);
}

static void teardown_storage_db(void) {
    (void)microdb_deinit(&g_db);
    microdb_port_posix_deinit(&g_storage);
    microdb_port_posix_remove(g_path);
}

MDB_TEST(test_inspect_empty_db) {
    microdb_stats_t stats;

    ASSERT_EQ(microdb_inspect(&g_db, &stats), MICRODB_OK);
    ASSERT_EQ(stats.kv_entries_used, 0u);
    ASSERT_EQ(stats.kv_collision_count, 0u);
    ASSERT_EQ(stats.kv_eviction_count, 0u);
    ASSERT_EQ(stats.ts_streams_registered, 0u);
    ASSERT_EQ(stats.ts_samples_total, 0u);
    ASSERT_EQ(stats.wal_bytes_used, 0u);
    ASSERT_EQ(stats.rel_tables_count, 0u);
    ASSERT_EQ(stats.rel_rows_total, 0u);
    ASSERT_EQ(stats.kv_fill_pct, 0u);
    ASSERT_EQ(stats.ts_fill_pct, 0u);
    ASSERT_EQ(stats.wal_fill_pct, 0u);
}

MDB_TEST(test_inspect_kv_entries) {
    microdb_stats_t stats;
    uint8_t value = 1u;
    uint32_t i;

    for (i = 0u; i < 8u; ++i) {
        char key[8] = { 0 };
        key[0] = 'k';
        key[1] = (char)('0' + (char)i);
        ASSERT_EQ(microdb_kv_set(&g_db, key, &value, 1u, 0u), MICRODB_OK);
    }

    ASSERT_EQ(microdb_inspect(&g_db, &stats), MICRODB_OK);
    ASSERT_EQ(stats.kv_entries_used, 8u);
}

MDB_TEST(test_inspect_kv_eviction) {
    microdb_stats_t stats;
    uint8_t value = 1u;
    uint32_t i;

    for (i = 0u; i < MICRODB_KV_MAX_KEYS + 8u; ++i) {
        char key[12] = { 0 };
        key[0] = 'e';
        key[1] = (char)('0' + (char)((i / 100u) % 10u));
        key[2] = (char)('0' + (char)((i / 10u) % 10u));
        key[3] = (char)('0' + (char)(i % 10u));
        ASSERT_EQ(microdb_kv_set(&g_db, key, &value, 1u, 0u), MICRODB_OK);
    }

    ASSERT_EQ(microdb_inspect(&g_db, &stats), MICRODB_OK);
    ASSERT_GT(stats.kv_eviction_count, 0u);
}

MDB_TEST(test_inspect_ts_samples) {
    microdb_stats_t stats;
    uint32_t i;

    ASSERT_EQ(microdb_ts_register(&g_db, "temp", MICRODB_TS_U32, 0u), MICRODB_OK);
    for (i = 0u; i < 6u; ++i) {
        uint32_t v = i + 1u;
        ASSERT_EQ(microdb_ts_insert(&g_db, "temp", (microdb_timestamp_t)i, &v), MICRODB_OK);
    }

    ASSERT_EQ(microdb_inspect(&g_db, &stats), MICRODB_OK);
    ASSERT_EQ(stats.ts_streams_registered, 1u);
    ASSERT_EQ(stats.ts_samples_total, 6u);
}

MDB_TEST(test_inspect_wal) {
    microdb_stats_t stats;
    uint8_t value = 7u;

    ASSERT_EQ(microdb_kv_set(&g_db, "wal", &value, 1u, 0u), MICRODB_OK);
    ASSERT_EQ(microdb_inspect(&g_db, &stats), MICRODB_OK);
    ASSERT_GT(stats.wal_bytes_used, 0u);
}

int main(void) {
    MDB_RUN_TEST(setup_db, teardown_db, test_inspect_empty_db);
    MDB_RUN_TEST(setup_db, teardown_db, test_inspect_kv_entries);
    MDB_RUN_TEST(setup_db, teardown_db, test_inspect_kv_eviction);
    MDB_RUN_TEST(setup_db, teardown_db, test_inspect_ts_samples);
    MDB_RUN_TEST(setup_storage_db, teardown_storage_db, test_inspect_wal);
    return MDB_RESULT();
}
