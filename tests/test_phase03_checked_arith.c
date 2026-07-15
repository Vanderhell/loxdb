// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "lox_arena.h"
#include "../port/ram/lox_port_ram.h"
#include "../src/lox_internal.h"

#include <stdlib.h>
#include <string.h>

static lox_t g_db_a;
static lox_t g_db_b;
static lox_storage_t g_storage_a;
static lox_storage_t g_storage_b;
static uint32_t g_sync_calls = 0u;
static uint32_t g_fail_sync_call = 0u;

static void setup_noop(void) {}
static void teardown_noop(void) {}

static lox_err_t tracking_sync(void *ctx) {
    g_sync_calls++;
    if (g_fail_sync_call != 0u && g_sync_calls == g_fail_sync_call) {
        return LOX_ERR_STORAGE;
    }
    (void)ctx;
    return LOX_OK;
}

static void setup_dual_db(void) {
    lox_cfg_t cfg;

    memset(&g_db_a, 0, sizeof(g_db_a));
    memset(&g_db_b, 0, sizeof(g_db_b));
    memset(&g_storage_a, 0, sizeof(g_storage_a));
    memset(&g_storage_b, 0, sizeof(g_storage_b));
    ASSERT_EQ(lox_port_ram_init(&g_storage_a, 131072u), LOX_OK);
    ASSERT_EQ(lox_port_ram_init(&g_storage_b, 65536u), LOX_OK);

    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage_a;
    cfg.ram_kb = 32u;
    ASSERT_EQ(lox_init(&g_db_a, &cfg), LOX_OK);

    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage_b;
    cfg.ram_kb = 32u;
    ASSERT_EQ(lox_init(&g_db_b, &cfg), LOX_OK);
}

static void teardown_dual_db(void) {
    if (lox_core_const(&g_db_a)->magic == LOX_MAGIC) {
        (void)lox_deinit(&g_db_a);
    }
    if (lox_core_const(&g_db_b)->magic == LOX_MAGIC) {
        (void)lox_deinit(&g_db_b);
    }
    lox_port_ram_deinit(&g_storage_a);
    lox_port_ram_deinit(&g_storage_b);
}

static void setup_persistent_db(void) {
    lox_cfg_t cfg;

    memset(&g_db_a, 0, sizeof(g_db_a));
    memset(&g_storage_a, 0, sizeof(g_storage_a));
    ASSERT_EQ(lox_port_ram_init(&g_storage_a, 65536u), LOX_OK);

    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage_a;
    cfg.ram_kb = 32u;
    ASSERT_EQ(lox_init(&g_db_a, &cfg), LOX_OK);
}

static void teardown_persistent_db(void) {
    if (lox_core_const(&g_db_a)->magic == LOX_MAGIC) {
        (void)lox_deinit(&g_db_a);
    }
    lox_port_ram_deinit(&g_storage_a);
}

static const lox_schema_impl_t *schema_impl(const lox_schema_t *schema) {
    return (const lox_schema_impl_t *)&schema->_opaque[0];
}

static void reopen_persistent_db(void) {
    lox_cfg_t cfg;

    ASSERT_EQ(lox_deinit(&g_db_a), LOX_OK);
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage_a;
    cfg.ram_kb = 32u;
    ASSERT_EQ(lox_init(&g_db_a, &cfg), LOX_OK);
}

static void setup_compact_db(void) {
    lox_cfg_t cfg;

    memset(&g_db_a, 0, sizeof(g_db_a));
    memset(&g_storage_a, 0, sizeof(g_storage_a));
    ASSERT_EQ(lox_port_ram_init(&g_storage_a, 65536u), LOX_OK);
    g_storage_a.sync = tracking_sync;
    g_sync_calls = 0u;
    g_fail_sync_call = 0u;

    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage_a;
    cfg.ram_kb = 32u;
    cfg.wal_compact_auto = 1u;
    cfg.wal_compact_threshold_pct = 1u;
    ASSERT_EQ(lox_init(&g_db_a, &cfg), LOX_OK);
    g_fail_sync_call = g_sync_calls + 2u;
}

static void teardown_compact_db(void) {
    if (lox_core_const(&g_db_a)->magic == LOX_MAGIC) {
        (void)lox_deinit(&g_db_a);
    }
    lox_port_ram_deinit(&g_storage_a);
}

MDB_TEST(arena_alloc_rejects_invalid_alignment) {
    uint8_t storage[32];
    lox_arena_t arena;

    memset(storage, 0xA5, sizeof(storage));
    lox_arena_init(&arena, storage, sizeof(storage));
    ASSERT_EQ(lox_arena_alloc(&arena, 4u, 0u), NULL);
    ASSERT_EQ(lox_arena_alloc(&arena, 4u, 3u), NULL);
}

MDB_TEST(arena_alloc_rejects_overflow_and_preserves_state) {
    uint8_t storage[32];
    lox_arena_t arena;
    void *ptr;

    memset(storage, 0xA5, sizeof(storage));
    lox_arena_init(&arena, storage, sizeof(storage));
    arena.used = SIZE_MAX - 3u;
    ptr = lox_arena_alloc(&arena, 8u, 8u);
    ASSERT_EQ(ptr, NULL);
    ASSERT_EQ(arena.used, SIZE_MAX - 3u);
}

MDB_TEST(arena_remaining_clamps_when_used_exceeds_capacity) {
    lox_arena_t arena;

    memset(&arena, 0, sizeof(arena));
    arena.capacity = 16u;
    arena.used = 32u;
    ASSERT_EQ(lox_arena_remaining(&arena), 0u);
}

MDB_TEST(ts_register_rejects_invalid_type_value) {
    lox_cfg_t cfg;

    memset(&g_db_a, 0, sizeof(g_db_a));
    memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 32u;
    ASSERT_EQ(lox_init(&g_db_a, &cfg), LOX_OK);
    ASSERT_EQ(lox_ts_register(&g_db_a, "bad", (lox_ts_type_t)99, 0u), LOX_ERR_INVALID);
    ASSERT_EQ(lox_deinit(&g_db_a), LOX_OK);
}

MDB_TEST(rel_table_handles_are_bound_to_their_owner_db) {
    lox_schema_t schema;
    lox_table_t *table = NULL;
    uint32_t id = 7u;
    uint8_t row[64] = { 0 };
    uint32_t count = 0u;

    ASSERT_EQ(lox_schema_init(&schema, "users", 8u), LOX_OK);
    ASSERT_EQ(lox_schema_add(&schema, "id", LOX_COL_U32, sizeof(uint32_t), true), LOX_OK);
    ASSERT_EQ(lox_schema_add(&schema, "age", LOX_COL_U8, sizeof(uint8_t), false), LOX_OK);
    ASSERT_EQ(lox_schema_seal(&schema), LOX_OK);
    ASSERT_EQ(lox_table_create(&g_db_a, &schema), LOX_OK);
    ASSERT_EQ(lox_table_get(&g_db_a, "users", &table), LOX_OK);
    ASSERT_EQ(lox_row_set(table, row, "id", &id), LOX_OK);
    ASSERT_EQ(lox_row_set(table, row, "age", &(uint8_t){ 3u }), LOX_OK);

    ASSERT_EQ(lox_rel_insert(&g_db_b, table, row), LOX_ERR_INVALID);
    ASSERT_EQ(lox_rel_count(table, &count), LOX_OK);
    ASSERT_EQ(count, 0u);
}

MDB_TEST(auto_compaction_failure_is_reported_in_runtime_status) {
    uint8_t value = 1u;
    lox_db_stats_t stats;

    ASSERT_EQ(lox_kv_set(&g_db_a, "hot", &value, 1u, 0u), LOX_OK);
    ASSERT_EQ(lox_get_db_stats(&g_db_a, &stats), LOX_OK);
    ASSERT_EQ(stats.last_runtime_error, LOX_ERR_STORAGE);
    ASSERT_EQ(lox_kv_get(&g_db_a, "hot", &value, 1u, NULL), LOX_OK);
    ASSERT_EQ(stats.compact_count, 0u);
}

MDB_TEST(kv_eviction_replay_is_deterministic_across_reopen) {
    uint8_t value = 1u;
    uint8_t out = 0u;
    char key[16];
    uint32_t i;

    for (i = 0u; i < LOX_KV_MAX_KEYS - LOX_TXN_STAGE_KEYS; ++i) {
        (void)snprintf(key, sizeof(key), "k%02u", (unsigned)i);
        ASSERT_EQ(lox_kv_put(&g_db_a, key, &value, sizeof(value)), LOX_OK);
    }
    ASSERT_EQ(lox_kv_put(&g_db_a, "evict", &value, sizeof(value)), LOX_OK);
    (void)snprintf(key, sizeof(key), "k%02u", 0u);
    ASSERT_EQ(lox_kv_get(&g_db_a, key, &out, sizeof(out), NULL), LOX_ERR_NOT_FOUND);

    reopen_persistent_db();
    ASSERT_EQ(lox_kv_get(&g_db_a, key, &out, sizeof(out), NULL), LOX_ERR_NOT_FOUND);
    ASSERT_EQ(lox_kv_get(&g_db_a, "evict", &out, sizeof(out), NULL), LOX_OK);
}

MDB_TEST(ts_retain_config_persists_across_reopen) {
    lox_ts_log_retain_cfg_t cfg;
    const lox_core_t *core;
    uint32_t value = 7u;
    uint32_t i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.log_retain_zones = 4u;
    cfg.log_retain_zone_pct = 25u;
    ASSERT_EQ(lox_ts_register_ex(&g_db_a, "temp", LOX_TS_U32, 0u, &cfg), LOX_OK);
    ASSERT_EQ(lox_ts_insert(&g_db_a, "temp", 10u, &value), LOX_OK);
    reopen_persistent_db();

    core = lox_core_const(&g_db_a);
    for (i = 0u; i < LOX_TS_MAX_STREAMS; ++i) {
        if (core->ts.streams[i].registered && strcmp(core->ts.streams[i].name, "temp") == 0) {
            ASSERT_EQ(core->ts.streams[i].log_retain_zones, 4u);
            ASSERT_EQ(core->ts.streams[i].log_retain_zone_pct, 25u);
            return;
        }
    }
    ASSERT_EQ(0, 1);
}

MDB_TEST(rel_table_create_rolls_back_arena_on_failed_alloc) {
    lox_schema_t schema;
    lox_core_t *core;
    size_t original_used;
    uint32_t before_tables;
    lox_err_t rc = LOX_ERR_NO_MEM;
    uint8_t *original_base;

    ASSERT_EQ(lox_schema_init(&schema, "wide", 256u), LOX_OK);
    ASSERT_EQ(lox_schema_add(&schema, "id", LOX_COL_U32, sizeof(uint32_t), true), LOX_OK);
    ASSERT_EQ(lox_schema_add(&schema, "payload", LOX_COL_BLOB, 32u, false), LOX_OK);
    ASSERT_EQ(lox_schema_seal(&schema), LOX_OK);

    core = lox_core(&g_db_a);
    before_tables = core->rel.registered_tables;
    original_used = core->rel_arena.used;
    original_base = core->rel_arena.base;
    core->rel_arena.base = NULL;
    rc = lox_table_create(&g_db_a, &schema);
    core->rel_arena.base = original_base;
    ASSERT_EQ(rc, LOX_ERR_NO_MEM);
    ASSERT_EQ(core->rel.registered_tables, before_tables);
    ASSERT_EQ(core->rel_arena.used, original_used);
    core->rel_arena.used = original_used;
}

int main(void) {
    MDB_RUN_TEST(setup_noop, teardown_noop, arena_alloc_rejects_invalid_alignment);
    MDB_RUN_TEST(setup_noop, teardown_noop, arena_alloc_rejects_overflow_and_preserves_state);
    MDB_RUN_TEST(setup_noop, teardown_noop, arena_remaining_clamps_when_used_exceeds_capacity);
    MDB_RUN_TEST(setup_noop, teardown_noop, ts_register_rejects_invalid_type_value);
    MDB_RUN_TEST(setup_dual_db, teardown_dual_db, rel_table_handles_are_bound_to_their_owner_db);
    MDB_RUN_TEST(setup_compact_db, teardown_compact_db, auto_compaction_failure_is_reported_in_runtime_status);
    MDB_RUN_TEST(setup_persistent_db, teardown_persistent_db, kv_eviction_replay_is_deterministic_across_reopen);
    MDB_RUN_TEST(setup_persistent_db, teardown_persistent_db, ts_retain_config_persists_across_reopen);
    MDB_RUN_TEST(setup_persistent_db, teardown_persistent_db, rel_table_create_rolls_back_arena_on_failed_alloc);
    return MDB_RESULT();
}
