#include "microtest.h"
#include "microdb_cpp.hpp"

#include <cstring>

static microdb::cpp::Database g_db;

static void setup_noop(void) {
}

static void setup_db(void) {
    microdb_cfg_t cfg;

    std::memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 32u;
    ASSERT_EQ(g_db.init(cfg), MICRODB_OK);
}

static void teardown_db(void) {
    if (g_db.initialized()) {
        ASSERT_EQ(g_db.deinit(), MICRODB_OK);
    }
}

MDB_TEST(cpp_wrapper_reports_invalid_before_init) {
    microdb::cpp::Database db;
    microdb_stats_t stats;

    ASSERT_EQ(db.initialized(), 0);
    ASSERT_EQ(db.handle() == nullptr, 1);
    ASSERT_EQ(db.flush(), MICRODB_ERR_INVALID);
    ASSERT_EQ(db.stats(&stats), MICRODB_ERR_INVALID);
}

MDB_TEST(cpp_wrapper_init_and_stats) {
    microdb_stats_t stats;
    microdb_db_stats_t dbs;

    ASSERT_EQ(g_db.initialized(), 1);
    ASSERT_EQ(g_db.handle() != nullptr, 1);
    ASSERT_EQ(g_db.stats(&stats), MICRODB_OK);
    ASSERT_EQ(g_db.db_stats(&dbs), MICRODB_OK);
    ASSERT_GT(stats.kv_entries_max, 0u);
    ASSERT_EQ(dbs.last_runtime_error, MICRODB_OK);
}

MDB_TEST(cpp_wrapper_handle_allows_core_api_usage) {
    uint8_t value = 7u;
    uint8_t out = 0u;

    ASSERT_EQ(microdb_kv_set(g_db.handle(), "cpp", &value, sizeof(value), 0u), MICRODB_OK);
    ASSERT_EQ(microdb_kv_get(g_db.handle(), "cpp", &out, sizeof(out), NULL), MICRODB_OK);
    ASSERT_EQ(out, value);
}

MDB_TEST(cpp_wrapper_prevents_double_init) {
    microdb_cfg_t cfg;

    std::memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 32u;
    ASSERT_EQ(g_db.init(cfg), MICRODB_ERR_INVALID);
}

int main(void) {
    MDB_RUN_TEST(setup_noop, teardown_db, cpp_wrapper_reports_invalid_before_init);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_init_and_stats);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_handle_allows_core_api_usage);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_prevents_double_init);
    return MDB_RESULT();
}
