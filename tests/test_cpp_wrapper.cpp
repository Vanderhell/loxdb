#include "microtest.h"
#include "microdb_cpp.hpp"

#include <cstring>

static microdb::cpp::Database g_db;
typedef struct {
    uint32_t count;
} iter_ctx_t;

static bool kv_iter_count_cb(const char *key, const void *val, size_t val_len, uint32_t ttl_remaining, void *ctx) {
    iter_ctx_t *it = (iter_ctx_t *)ctx;
    (void)key;
    (void)val;
    (void)val_len;
    (void)ttl_remaining;
    it->count++;
    return true;
}

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

MDB_TEST(cpp_wrapper_kv_set_get_del_exists) {
    uint8_t value = 42u;
    uint8_t out = 0u;
    size_t out_len = 0u;

    ASSERT_EQ(g_db.kv_set("k", &value, sizeof(value), 0u), MICRODB_OK);
    ASSERT_EQ(g_db.kv_exists("k"), MICRODB_OK);
    ASSERT_EQ(g_db.kv_get("k", &out, sizeof(out), &out_len), MICRODB_OK);
    ASSERT_EQ(out_len, 1u);
    ASSERT_EQ(out, value);
    ASSERT_EQ(g_db.kv_del("k"), MICRODB_OK);
    ASSERT_EQ(g_db.kv_exists("k"), MICRODB_ERR_NOT_FOUND);
}

MDB_TEST(cpp_wrapper_kv_iter_and_clear) {
    uint8_t value = 1u;
    iter_ctx_t it;

    ASSERT_EQ(g_db.kv_put("a", &value, 1u), MICRODB_OK);
    ASSERT_EQ(g_db.kv_put("b", &value, 1u), MICRODB_OK);
    ASSERT_EQ(g_db.kv_put("c", &value, 1u), MICRODB_OK);
    it.count = 0u;
    ASSERT_EQ(g_db.kv_iter(kv_iter_count_cb, &it), MICRODB_OK);
    ASSERT_EQ(it.count, 3u);
    ASSERT_EQ(g_db.kv_clear(), MICRODB_OK);
    it.count = 0u;
    ASSERT_EQ(g_db.kv_iter(kv_iter_count_cb, &it), MICRODB_OK);
    ASSERT_EQ(it.count, 0u);
}

MDB_TEST(cpp_wrapper_admit_kv_set) {
    microdb_admission_t a;
    uint8_t value = 5u;

    ASSERT_EQ(g_db.admit_kv_set("admit", 1u, &a), MICRODB_OK);
    ASSERT_EQ(a.status, MICRODB_OK);
    ASSERT_EQ(g_db.kv_put("admit", &value, 1u), MICRODB_OK);
}

int main(void) {
    MDB_RUN_TEST(setup_noop, teardown_db, cpp_wrapper_reports_invalid_before_init);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_init_and_stats);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_handle_allows_core_api_usage);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_prevents_double_init);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_kv_set_get_del_exists);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_kv_iter_and_clear);
    MDB_RUN_TEST(setup_db, teardown_db, cpp_wrapper_admit_kv_set);
    return MDB_RESULT();
}
