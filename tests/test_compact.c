#include "microtest.h"
#include "microdb.h"
#include "../port/posix/microdb_port_posix.h"
#include "../src/microdb_crc.h"

#include <string.h>

enum {
    WAL_MAGIC = 0x4D44424Cu,
    WAL_VERSION = 0x00010000u,
    WAL_ENTRY_MAGIC = 0x454E5452u,
    WAL_ENGINE_META = 0xFFu,
    WAL_OP_COMPACT_BEGIN = 3u
};

static microdb_t g_db;
static microdb_storage_t g_storage;
static const char *g_path = "compact_test.bin";

static void put_u32(uint8_t *dst, uint32_t value) {
    memcpy(dst, &value, sizeof(value));
}

static void put_u16(uint8_t *dst, uint16_t value) {
    memcpy(dst, &value, sizeof(value));
}

static void open_db_with_cfg(const microdb_cfg_t *cfg) {
    ASSERT_EQ(microdb_port_posix_init(&g_storage, g_path, 65536u), MICRODB_OK);
    ASSERT_EQ(microdb_init(&g_db, cfg), MICRODB_OK);
}

static void setup_db(void) {
    microdb_cfg_t cfg;

    microdb_port_posix_remove(g_path);
    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    open_db_with_cfg(&cfg);
}

static void setup_auto_db(void) {
    microdb_cfg_t cfg;

    microdb_port_posix_remove(g_path);
    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    cfg.wal_compact_auto = 1u;
    cfg.wal_compact_threshold_pct = 50u;
    open_db_with_cfg(&cfg);
}

static void teardown_db(void) {
    (void)microdb_deinit(&g_db);
    microdb_port_posix_deinit(&g_storage);
    microdb_port_posix_remove(g_path);
}

MDB_TEST(test_manual_compact_resets_wal) {
    microdb_stats_t stats;
    uint8_t value = 1u;
    uint32_t i;

    for (i = 0u; i < 128u; ++i) {
        char key[12] = { 0 };
        key[0] = 'k';
        key[1] = (char)('0' + (char)((i / 100u) % 10u));
        key[2] = (char)('0' + (char)((i / 10u) % 10u));
        key[3] = (char)('0' + (char)(i % 10u));
        ASSERT_EQ(microdb_kv_set(&g_db, key, &value, 1u, 0u), MICRODB_OK);
        ASSERT_EQ(microdb_inspect(&g_db, &stats), MICRODB_OK);
        if (stats.wal_fill_pct > 50u) {
            break;
        }
    }

    ASSERT_GT(stats.wal_fill_pct, 50u);
    ASSERT_EQ(microdb_compact(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_inspect(&g_db, &stats), MICRODB_OK);
    ASSERT_EQ(stats.wal_fill_pct < 5u, 1);
}

MDB_TEST(test_data_intact_after_compact) {
    uint8_t a = 1u;
    uint8_t b = 2u;
    uint8_t c = 3u;
    uint8_t out = 0u;

    ASSERT_EQ(microdb_kv_set(&g_db, "A", &a, 1u, 0u), MICRODB_OK);
    ASSERT_EQ(microdb_kv_set(&g_db, "B", &b, 1u, 0u), MICRODB_OK);
    ASSERT_EQ(microdb_kv_set(&g_db, "C", &c, 1u, 0u), MICRODB_OK);
    ASSERT_EQ(microdb_compact(&g_db), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 1u);
    ASSERT_EQ(microdb_kv_get(&g_db, "B", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 2u);
    ASSERT_EQ(microdb_kv_get(&g_db, "C", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 3u);
}

MDB_TEST(test_auto_compact_fires) {
    microdb_stats_t stats;
    uint8_t value = 7u;
    uint32_t prev_fill = 0u;
    uint32_t i;
    uint32_t fired = 0u;

    for (i = 0u; i < 256u; ++i) {
        char key[12] = { 0 };
        key[0] = 'a';
        key[1] = (char)('0' + (char)((i / 100u) % 10u));
        key[2] = (char)('0' + (char)((i / 10u) % 10u));
        key[3] = (char)('0' + (char)(i % 10u));
        ASSERT_EQ(microdb_kv_set(&g_db, key, &value, 1u, 0u), MICRODB_OK);
        ASSERT_EQ(microdb_inspect(&g_db, &stats), MICRODB_OK);
        if (prev_fill >= 40u && stats.wal_fill_pct < 5u) {
            fired = 1u;
            break;
        }
        prev_fill = stats.wal_fill_pct;
    }

    ASSERT_EQ(fired, 1u);
}

MDB_TEST(test_recovery_after_interrupted_compact) {
    microdb_cfg_t cfg;
    uint8_t a = 1u;
    uint8_t b = 2u;
    uint8_t c = 3u;
    uint8_t out = 0u;
    uint8_t wal_header[32];
    uint8_t wal_entry[16];
    uint32_t crc;

    ASSERT_EQ(microdb_kv_set(&g_db, "A", &a, 1u, 0u), MICRODB_OK);
    ASSERT_EQ(microdb_kv_set(&g_db, "B", &b, 1u, 0u), MICRODB_OK);
    ASSERT_EQ(microdb_kv_set(&g_db, "C", &c, 1u, 0u), MICRODB_OK);
    ASSERT_EQ(microdb_flush(&g_db), MICRODB_OK);

    memset(wal_header, 0, sizeof(wal_header));
    put_u32(wal_header + 0u, WAL_MAGIC);
    put_u32(wal_header + 4u, WAL_VERSION);
    put_u32(wal_header + 8u, 1u);
    put_u32(wal_header + 12u, 1u);
    crc = MICRODB_CRC32(wal_header, 16u);
    put_u32(wal_header + 16u, crc);
    ASSERT_EQ(g_storage.write(g_storage.ctx, 0u, wal_header, sizeof(wal_header)), MICRODB_OK);

    memset(wal_entry, 0, sizeof(wal_entry));
    put_u32(wal_entry + 0u, WAL_ENTRY_MAGIC);
    put_u32(wal_entry + 4u, 1u);
    wal_entry[8] = (uint8_t)WAL_ENGINE_META;
    wal_entry[9] = (uint8_t)WAL_OP_COMPACT_BEGIN;
    put_u16(wal_entry + 10u, 0u);
    crc = MICRODB_CRC32(wal_entry, 12u);
    put_u32(wal_entry + 12u, crc);
    ASSERT_EQ(g_storage.write(g_storage.ctx, 32u, wal_entry, sizeof(wal_entry)), MICRODB_OK);
    ASSERT_EQ(g_storage.sync(g_storage.ctx), MICRODB_OK);

    ASSERT_EQ(microdb_deinit(&g_db), MICRODB_OK);
    microdb_port_posix_deinit(&g_storage);

    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    open_db_with_cfg(&cfg);

    ASSERT_EQ(microdb_kv_get(&g_db, "A", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 1u);
    ASSERT_EQ(microdb_kv_get(&g_db, "B", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 2u);
    ASSERT_EQ(microdb_kv_get(&g_db, "C", &out, 1u, NULL), MICRODB_OK);
    ASSERT_EQ(out, 3u);
}

int main(void) {
    MDB_RUN_TEST(setup_db, teardown_db, test_manual_compact_resets_wal);
    MDB_RUN_TEST(setup_db, teardown_db, test_data_intact_after_compact);
    MDB_RUN_TEST(setup_auto_db, teardown_db, test_auto_compact_fires);
    MDB_RUN_TEST(setup_db, teardown_db, test_recovery_after_interrupted_compact);
    return MDB_RESULT();
}
