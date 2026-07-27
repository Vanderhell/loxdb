// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "lox_port_ram.h"
#include "../src/lox_internal.h"

#include <string.h>

static lox_err_t st_read(void *ctx, uint32_t off, void *buf, size_t len) {
    (void)ctx; (void)off; (void)buf; (void)len;
    return LOX_OK;
}

static lox_err_t st_write(void *ctx, uint32_t off, const void *buf, size_t len) {
    (void)ctx; (void)off; (void)buf; (void)len;
    return LOX_OK;
}

static lox_err_t st_erase(void *ctx, uint32_t off) {
    (void)ctx; (void)off;
    return LOX_OK;
}

static lox_err_t st_sync(void *ctx) {
    (void)ctx;
    return LOX_OK;
}

static void noop(void) {
}

MDB_TEST(test_preflight_null_args_invalid) {
    lox_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    ASSERT_EQ(lox_preflight(NULL, NULL), LOX_ERR_INVALID);
    ASSERT_EQ(lox_preflight(&cfg, NULL), LOX_ERR_INVALID);
}

MDB_TEST(test_preflight_ram_only_ok) {
    lox_cfg_t cfg;
    lox_preflight_report_t rep;
    memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 64u;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_OK);
    ASSERT_EQ(rep.status, LOX_OK);
    ASSERT_EQ(rep.ram_kb, 64u);
#if LOX_ENABLE_KV
    ASSERT_GT(rep.kv_arena_bytes, 0u);
#else
    ASSERT_EQ(rep.kv_arena_bytes, 0u);
#endif
#if LOX_ENABLE_TS
    ASSERT_GT(rep.ts_arena_bytes, 0u);
#else
    ASSERT_EQ(rep.ts_arena_bytes, 0u);
#endif
#if LOX_ENABLE_REL
    ASSERT_GT(rep.rel_arena_bytes, 0u);
#else
    ASSERT_EQ(rep.rel_arena_bytes, 0u);
#endif
}

MDB_TEST(test_preflight_pct_invalid) {
    lox_cfg_t cfg;
    lox_preflight_report_t rep;
    memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 64u;
    cfg.kv_pct = 50u;
    cfg.ts_pct = 50u;
    cfg.rel_pct = 1u;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_ERR_INVALID);
}

MDB_TEST(test_preflight_storage_contract_invalid) {
    lox_cfg_t cfg;
    lox_storage_t st;
    lox_preflight_report_t rep;
    memset(&cfg, 0, sizeof(cfg));
    memset(&st, 0, sizeof(st));
    st.read = st_read;
    st.write = st_write;
    st.erase = st_erase;
    st.sync = st_sync;
    st.capacity = 1024u * 1024u;
    st.erase_size = 4096u;
    st.write_size = 2u;
    cfg.ram_kb = 64u;
    cfg.storage = &st;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_ERR_INVALID);
}

MDB_TEST(test_preflight_storage_too_small) {
    lox_cfg_t cfg;
    lox_storage_t st;
    lox_preflight_report_t rep;
    memset(&cfg, 0, sizeof(cfg));
    memset(&st, 0, sizeof(st));
    st.read = st_read;
    st.write = st_write;
    st.erase = st_erase;
    st.sync = st_sync;
    st.capacity = 1u;
    st.erase_size = 4096u;
    st.write_size = 1u;
    cfg.ram_kb = 64u;
    cfg.storage = &st;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_ERR_STORAGE);
    ASSERT_EQ(rep.status, LOX_ERR_STORAGE);
    ASSERT_GT(rep.storage_required_bytes, st.capacity);
}

MDB_TEST(test_preflight_storage_ok) {
    lox_cfg_t cfg;
    lox_storage_t st;
    lox_preflight_report_t rep;
    memset(&cfg, 0, sizeof(cfg));
    memset(&st, 0, sizeof(st));
    st.read = st_read;
    st.write = st_write;
    st.erase = st_erase;
    st.sync = st_sync;
    st.capacity = 16u * 1024u * 1024u;
    st.erase_size = 4096u;
    st.write_size = 1u;
    cfg.ram_kb = 64u;
    cfg.storage = &st;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_OK);
    ASSERT_EQ(rep.status, LOX_OK);
    ASSERT_GT(rep.storage_required_bytes, 0u);
    ASSERT_LE(rep.storage_required_bytes, st.capacity);
    ASSERT_GT(rep.wal_size, 0u);
}

static void expected_default_split(uint8_t *kv, uint8_t *ts, uint8_t *rel) {
    uint32_t weights[3] = {LOX_RAM_KV_PCT, LOX_RAM_TS_PCT, LOX_RAM_REL_PCT};
    uint32_t enabled[3] = {LOX_ENABLE_KV, LOX_ENABLE_TS, LOX_ENABLE_REL};
    uint32_t values[3] = {0u, 0u, 0u};
    uint32_t total = 0u;
    uint32_t assigned = 0u;
    uint32_t first = 3u;
    uint32_t i;
    for (i = 0u; i < 3u; ++i) {
        if (enabled[i] != 0u) {
            if (first == 3u) first = i;
            total += weights[i];
        }
    }
    for (i = 0u; i < 3u; ++i) {
        if (enabled[i] != 0u) {
            values[i] = (weights[i] * 100u) / total;
            assigned += values[i];
        }
    }
    values[first] += 100u - assigned;
    *kv = (uint8_t)values[0];
    *ts = (uint8_t)values[1];
    *rel = (uint8_t)values[2];
}

MDB_TEST(test_effective_split_matches_enabled_engines) {
    lox_cfg_t cfg;
    lox_preflight_report_t rep;
    uint8_t kv;
    uint8_t ts;
    uint8_t rel;
    uint32_t arena_sum;
    memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 64u;
    expected_default_split(&kv, &ts, &rel);
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_OK);
    ASSERT_EQ(rep.kv_pct, kv);
    ASSERT_EQ(rep.ts_pct, ts);
    ASSERT_EQ(rep.rel_pct, rel);
#if LOX_ENABLE_KV
    ASSERT_GT(rep.kv_arena_bytes, 0u);
#else
    ASSERT_EQ(rep.kv_arena_bytes, 0u);
#endif
#if LOX_ENABLE_TS
    ASSERT_GT(rep.ts_arena_bytes, 0u);
#else
    ASSERT_EQ(rep.ts_arena_bytes, 0u);
#endif
#if LOX_ENABLE_REL
    ASSERT_GT(rep.rel_arena_bytes, 0u);
#else
    ASSERT_EQ(rep.rel_arena_bytes, 0u);
#endif
    arena_sum = rep.kv_arena_bytes + rep.ts_arena_bytes + rep.rel_arena_bytes;
    ASSERT_LE(arena_sum, rep.heap_total_bytes);
    ASSERT_LE(rep.heap_total_bytes - arena_sum, 2u * (uint32_t)sizeof(void *) - 1u);
}

static void set_valid_custom_split(lox_cfg_t *cfg) {
    uint32_t count = LOX_ENABLE_KV + LOX_ENABLE_TS + LOX_ENABLE_REL;
    uint32_t base = 100u / count;
    uint32_t remainder = 100u - base * count;
    if (LOX_ENABLE_KV) {
        cfg->kv_pct = (uint8_t)(base + remainder);
        remainder = 0u;
    }
    if (LOX_ENABLE_TS) {
        cfg->ts_pct = (uint8_t)(base + remainder);
        remainder = 0u;
    }
    if (LOX_ENABLE_REL) {
        cfg->rel_pct = (uint8_t)(base + remainder);
    }
}

MDB_TEST(test_custom_split_valid_and_invalid_match_init) {
    lox_cfg_t cfg;
    lox_preflight_report_t rep;
    lox_t db;
    memset(&cfg, 0, sizeof(cfg));
    memset(&db, 0, sizeof(db));
    cfg.ram_kb = 64u;
    set_valid_custom_split(&cfg);
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_OK);
    ASSERT_EQ(lox_init(&db, &cfg), LOX_OK);
    ASSERT_EQ(lox_deinit(&db), LOX_OK);

#if !LOX_ENABLE_KV
    cfg.kv_pct = 1u;
#elif !LOX_ENABLE_TS
    cfg.ts_pct = 1u;
#elif !LOX_ENABLE_REL
    cfg.rel_pct = 1u;
#else
    cfg.kv_pct = 0u;
#endif
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_ERR_INVALID);
    ASSERT_EQ(lox_init(&db, &cfg), LOX_ERR_INVALID);
}

static void run_capacity_case(uint32_t declared_capacity, lox_err_t expected) {
    lox_storage_t storage;
    lox_cfg_t cfg;
    lox_t db;
    memset(&storage, 0, sizeof(storage));
    memset(&cfg, 0, sizeof(cfg));
    memset(&db, 0, sizeof(db));
    ASSERT_EQ(lox_port_ram_init(&storage, 2u * 1024u * 1024u), LOX_OK);
    storage.capacity = declared_capacity;
    cfg.storage = &storage;
    cfg.ram_kb = 64u;
    ASSERT_EQ(lox_init(&db, &cfg), expected);
    if (expected == LOX_OK) {
        ASSERT_EQ(lox_deinit(&db), LOX_OK);
    }
    lox_port_ram_deinit(&storage);
}

MDB_TEST(test_preflight_init_capacity_boundaries_and_live_layout) {
    lox_storage_t storage;
    lox_cfg_t cfg;
    lox_preflight_report_t rep;
    lox_t db;
    const lox_storage_layout_t *live;
    uint32_t required;
    memset(&storage, 0, sizeof(storage));
    memset(&cfg, 0, sizeof(cfg));
    memset(&db, 0, sizeof(db));
    ASSERT_EQ(lox_port_ram_init(&storage, 2u * 1024u * 1024u), LOX_OK);
    cfg.storage = &storage;
    cfg.ram_kb = 64u;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_OK);
    required = rep.storage_required_bytes;
    ASSERT_GT(required, 0u);
    storage.capacity = required - 1u;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_ERR_STORAGE);
    ASSERT_EQ(rep.storage_required_bytes, required);
    lox_port_ram_deinit(&storage);
    run_capacity_case(required - 1u, LOX_ERR_STORAGE);
    run_capacity_case(required, LOX_OK);
    run_capacity_case(required + 256u, LOX_OK);

    ASSERT_EQ(lox_port_ram_init(&storage, 2u * 1024u * 1024u), LOX_OK);
    cfg.storage = &storage;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_OK);
    ASSERT_EQ(lox_init(&db, &cfg), LOX_OK);
    live = &lox_core_const(&db)->layout;
    ASSERT_EQ(rep.wal_offset, live->wal_offset);
    ASSERT_EQ(rep.wal_size, live->wal_size);
    ASSERT_EQ(rep.super_a_offset, live->super_a_offset);
    ASSERT_EQ(rep.super_b_offset, live->super_b_offset);
    ASSERT_EQ(rep.superblock_bytes, live->super_size);
    ASSERT_EQ(rep.bank_a_offset, live->bank_a_offset);
    ASSERT_EQ(rep.bank_b_offset, live->bank_b_offset);
    ASSERT_EQ(rep.bank_size, live->bank_size);
    ASSERT_EQ(rep.kv_snapshot_bytes, live->kv_size);
    ASSERT_EQ(rep.ts_snapshot_bytes, live->ts_size);
    ASSERT_EQ(rep.rel_snapshot_bytes, live->rel_size);
    ASSERT_EQ(rep.storage_layout_bytes, live->total_size);
    ASSERT_EQ(lox_deinit(&db), LOX_OK);
    lox_port_ram_deinit(&storage);
}

MDB_TEST(test_preflight_rejects_storage_arithmetic_overflow) {
    lox_cfg_t cfg;
    lox_storage_t storage;
    lox_preflight_report_t rep;
    memset(&cfg, 0, sizeof(cfg));
    memset(&storage, 0, sizeof(storage));
    storage.read = st_read;
    storage.write = st_write;
    storage.erase = st_erase;
    storage.sync = st_sync;
    storage.capacity = UINT32_MAX;
    storage.erase_size = UINT32_MAX;
    storage.write_size = 1u;
    cfg.storage = &storage;
    cfg.ram_kb = 64u;
    ASSERT_EQ(lox_preflight(&cfg, &rep), LOX_ERR_OVERFLOW);
    ASSERT_EQ(lox_init(&(lox_t){0}, &cfg), LOX_ERR_OVERFLOW);
}

int main(void) {
    MDB_RUN_TEST(noop, noop, test_preflight_null_args_invalid);
    MDB_RUN_TEST(noop, noop, test_preflight_ram_only_ok);
    MDB_RUN_TEST(noop, noop, test_preflight_pct_invalid);
    MDB_RUN_TEST(noop, noop, test_preflight_storage_contract_invalid);
    MDB_RUN_TEST(noop, noop, test_preflight_storage_too_small);
    MDB_RUN_TEST(noop, noop, test_preflight_storage_ok);
    MDB_RUN_TEST(noop, noop, test_effective_split_matches_enabled_engines);
    MDB_RUN_TEST(noop, noop, test_custom_split_valid_and_invalid_match_init);
    MDB_RUN_TEST(noop, noop, test_preflight_init_capacity_boundaries_and_live_layout);
    MDB_RUN_TEST(noop, noop, test_preflight_rejects_storage_arithmetic_overflow);
    return MDB_RESULT();
}
