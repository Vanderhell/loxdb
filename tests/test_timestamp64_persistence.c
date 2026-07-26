// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "lox_port_ram.h"

#include <stdint.h>
#include <string.h>

static lox_t g_db;
static lox_storage_t g_storage;
static uint64_t g_now = UINT64_C(0x100000000) + 100u;

static lox_timestamp_t mock_now(void) {
    return (lox_timestamp_t)g_now;
}

static lox_err_t open_db(void) {
    lox_cfg_t cfg;
    memset(&g_db, 0, sizeof(g_db));
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    cfg.now = mock_now;
    return lox_init(&g_db, &cfg);
}

static void noop(void) {
}

MDB_TEST(timestamp_above_u32_survives_snapshot_reopen) {
    uint8_t value = 0x5Au;
    uint8_t out = 0u;

    memset(&g_storage, 0, sizeof(g_storage));
    ASSERT_EQ(lox_port_ram_init(&g_storage, 65536u), LOX_OK);
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_kv_set(&g_db, "wide", &value, 1u, 60u), LOX_OK);
    ASSERT_EQ(lox_flush(&g_db), LOX_OK);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_kv_get(&g_db, "wide", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, value);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
    lox_port_ram_deinit(&g_storage);
}

int main(void) {
    MDB_RUN_TEST(noop, noop, timestamp_above_u32_survives_snapshot_reopen);
    return MDB_RESULT();
}
