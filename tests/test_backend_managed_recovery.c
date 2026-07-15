// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "lox_backend_adapter.h"
#include "lox_backend_open.h"
#include "strict_nor_emulator.h"
#include "../src/lox_internal.h"

#include <stdlib.h>
#include <string.h>

int lox_backend_nand_stub_register(void);

enum {
    MANAGED_CAPACITY = 131072u,
    MANAGED_ERASE_SIZE = 4096u
};

static nor_flash_ctx_t g_media;
static lox_storage_t g_raw_storage;
static lox_storage_t *g_effective_storage = NULL;
static lox_backend_open_session_t g_open_session;
static lox_t g_db;
static uint32_t g_now = 1000u;

static lox_timestamp_t mock_now(void) {
    return g_now++;
}

static void power_loss_reset_to_durable(void) {
    nor_flash_power_loss(&g_media);
}

static void managed_open_db(void) {
    lox_cfg_t cfg;

    memset(&g_db, 0, sizeof(g_db));
    g_effective_storage = NULL;
    ASSERT_EQ(lox_backend_open_prepare("nand_stub", &g_raw_storage, 0u, 1u, &g_open_session, &g_effective_storage), LOX_OK);
    ASSERT_EQ(g_open_session.using_managed_adapter, 1u);
    ASSERT_EQ(g_effective_storage != NULL, 1);

    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = g_effective_storage;
    cfg.ram_kb = 32u;
    cfg.now = mock_now;
    ASSERT_EQ(lox_init(&g_db, &cfg), LOX_OK);
}

static void managed_close_db_clean(void) {
    if (lox_core_const(&g_db)->magic == LOX_MAGIC) {
        ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
    }
    lox_backend_open_release(&g_open_session);
    g_effective_storage = NULL;
    memset(&g_db, 0, sizeof(g_db));
}

static void managed_crash_reopen(void) {
    if (lox_core_const(&g_db)->magic == LOX_MAGIC) {
        free(lox_core(&g_db)->heap);
    }
    lox_backend_open_release(&g_open_session);
    memset(&g_db, 0, sizeof(g_db));
    managed_open_db();
}

static void setup_fixture(void) {
    nor_flash_reset(&g_media);
    memset(&g_raw_storage, 0, sizeof(g_raw_storage));
    memset(&g_open_session, 0, sizeof(g_open_session));
    g_now = 1000u;

    nor_flash_bind_storage(&g_raw_storage, &g_media, MANAGED_ERASE_SIZE, 1u);

    lox_backend_registry_reset();
    ASSERT_EQ(lox_backend_nand_stub_register(), 0);
    managed_open_db();
}

static void teardown_fixture(void) {
    if (lox_core_const(&g_db)->magic == LOX_MAGIC) {
        (void)lox_deinit(&g_db);
    }
    lox_backend_open_release(&g_open_session);
    lox_backend_registry_reset();
}

MDB_TEST(managed_recovery_wal_replays_after_power_loss) {
    uint8_t in = 77u;
    uint8_t out = 0u;

    ASSERT_EQ(lox_kv_set(&g_db, "k", &in, 1u, 0u), LOX_OK);
    power_loss_reset_to_durable();
    managed_crash_reopen();
    ASSERT_EQ(lox_kv_get(&g_db, "k", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, 77u);
}

MDB_TEST(managed_recovery_sync_failure_does_not_commit_new_value) {
    uint8_t in = 11u;
    uint8_t out = 0u;

    g_media.fail_next_sync = 1u;
    ASSERT_EQ(lox_kv_set(&g_db, "volatile", &in, 1u, 0u), LOX_ERR_STORAGE);
    power_loss_reset_to_durable();
    managed_crash_reopen();
    ASSERT_EQ(lox_kv_get(&g_db, "volatile", &out, 1u, NULL), LOX_ERR_NOT_FOUND);
}

MDB_TEST(managed_recovery_clean_reopen_preserves_data) {
    uint8_t in = 33u;
    uint8_t out = 0u;

    ASSERT_EQ(lox_kv_set(&g_db, "stable", &in, 1u, 0u), LOX_OK);
    managed_close_db_clean();
    managed_open_db();
    ASSERT_EQ(lox_kv_get(&g_db, "stable", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, 33u);
}

int main(void) {
    MDB_RUN_TEST(setup_fixture, teardown_fixture, managed_recovery_wal_replays_after_power_loss);
    MDB_RUN_TEST(setup_fixture, teardown_fixture, managed_recovery_sync_failure_does_not_commit_new_value);
    MDB_RUN_TEST(setup_fixture, teardown_fixture, managed_recovery_clean_reopen_preserves_data);
    return MDB_RESULT();
}
