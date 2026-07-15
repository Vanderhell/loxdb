// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "lox_crc.h"
#include "strict_nor_emulator.h"
#include "../src/lox_internal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum {
    PHASE02_CAPACITY = 131072u,
    PHASE02_ERASE_SIZE = 4096u
};

static nor_flash_ctx_t g_media;
static lox_storage_t g_storage;
static lox_t g_db;
static uint32_t g_now = 3000u;

static lox_timestamp_t mock_now(void) {
    return g_now++;
}

static lox_err_t init_db(bool reset_media) {
    lox_cfg_t cfg;
    lox_err_t rc;

    if (reset_media) {
        nor_flash_reset(&g_media);
    }

    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    nor_flash_bind_storage(&g_storage, &g_media, PHASE02_ERASE_SIZE, 1u);

    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    cfg.now = mock_now;
    cfg.wal_sync_mode = LOX_WAL_SYNC_ALWAYS;
    rc = lox_init(&g_db, &cfg);
    if (rc != LOX_OK) {
    }
    return rc;
}

static void open_clean_db(void) {
    ASSERT_EQ(init_db(true), LOX_OK);
}

static lox_err_t reopen_after_power_loss(void) {
    if (lox_core_const(&g_db)->magic == LOX_MAGIC) {
        free(lox_core(&g_db)->heap);
    }
    memset(&g_db, 0, sizeof(g_db));
    nor_flash_power_loss(&g_media);
    return init_db(false);
}

static void close_db_clean(void) {
    if (lox_core_const(&g_db)->magic == LOX_MAGIC) {
        ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
    }
    memset(&g_db, 0, sizeof(g_db));
}

static uint8_t storage_read_u8(uint32_t offset) {
    uint8_t value = 0u;
    if (g_storage.read(g_storage.ctx, offset, &value, sizeof(value)) != LOX_OK) {
        mdb_test_failures++;
        return 0u;
    }
    return value;
}

static uint16_t storage_read_u16(uint32_t offset) {
    uint16_t value = 0u;
    if (g_storage.read(g_storage.ctx, offset, &value, sizeof(value)) != LOX_OK) {
        mdb_test_failures++;
        return 0u;
    }
    return value;
}

static uint32_t storage_read_u32(uint32_t offset) {
    uint32_t value = 0u;
    if (g_storage.read(g_storage.ctx, offset, &value, sizeof(value)) != LOX_OK) {
        mdb_test_failures++;
        return 0u;
    }
    return value;
}

static void storage_force_patch(uint32_t offset, const void *buf, size_t len) {
    ASSERT_LE((uint64_t)offset + (uint64_t)len, PHASE02_CAPACITY);
    memcpy(g_media.durable + offset, buf, len);
    memcpy(g_media.working + offset, buf, len);
}

static uint32_t wal_data_offset(void) {
    return g_storage.erase_size;
}

static uint32_t wal_entry_next_offset(uint32_t entry_offset) {
    uint16_t data_len = storage_read_u16(entry_offset + 10u);
    return entry_offset + 16u + (((uint32_t)data_len + 3u) & ~3u);
}

static bool find_wal_entry(uint8_t engine, uint8_t op, uint32_t *out_entry_offset) {
    uint32_t offset = wal_data_offset();
    while (offset + 16u <= g_storage.capacity) {
        uint32_t magic = storage_read_u32(offset);
        uint32_t data_len;
        uint32_t next_offset;

        if (magic != 0x454E5452u) {
            break;
        }

        data_len = storage_read_u16(offset + 10u);
        next_offset = offset + 16u + (((data_len + 3u) & ~3u));
        if (storage_read_u8(offset + 8u) == engine && storage_read_u8(offset + 9u) == op) {
            *out_entry_offset = offset;
            return true;
        }
        offset = next_offset;
    }
    return false;
}

static void write_raw_wal_entry(uint8_t engine, uint8_t op, const uint8_t *payload, uint16_t payload_len) {
    uint8_t header[16];
    uint8_t padded[1600];
    uint32_t crc;
    uint32_t aligned_len = ((uint32_t)payload_len + 3u) & ~3u;
    uint32_t entry_offset = wal_data_offset();
    uint32_t magic = 0x454E5452u;
    uint32_t seq = 1u;

    memset(header, 0, sizeof(header));
    memset(padded, 0xFF, sizeof(padded));
    memcpy(header + 0u, &magic, sizeof(magic));
    memcpy(header + 4u, &seq, sizeof(seq));
    header[8] = engine;
    header[9] = op;
    memcpy(header + 10u, &payload_len, sizeof(payload_len));
    crc = LOX_CRC32(header, 12u);
    crc = lox_crc32(crc, payload, payload_len);
    memcpy(header + 12u, &crc, sizeof(crc));

    memcpy(padded, header, sizeof(header));
    memcpy(padded + sizeof(header), payload, payload_len);
    ASSERT_EQ(g_storage.write(g_storage.ctx, entry_offset, padded, sizeof(header) + aligned_len), LOX_OK);
    ASSERT_EQ(g_storage.sync(g_storage.ctx), LOX_OK);
}

static void build_txn_set_payload(uint8_t *payload,
                                 uint32_t txid,
                                 const char *key,
                                 const uint8_t *value,
                                 size_t value_len,
                                 uint32_t expires_at,
                                 uint16_t *out_len) {
    size_t key_len = strlen(key);
    uint32_t val_len_u32 = (uint32_t)value_len;
    payload[0] = (uint8_t)(txid & 0xFFu);
    payload[1] = (uint8_t)((txid >> 8u) & 0xFFu);
    payload[2] = (uint8_t)((txid >> 16u) & 0xFFu);
    payload[3] = (uint8_t)((txid >> 24u) & 0xFFu);
    payload[4] = (uint8_t)key_len;
    memcpy(payload + 5u, key, key_len);
    memcpy(payload + 5u + key_len, &val_len_u32, sizeof(val_len_u32));
    memcpy(payload + 5u + key_len + 4u, value, value_len);
    memcpy(payload + 5u + key_len + 4u + value_len, &expires_at, sizeof(expires_at));
    *out_len = (uint16_t)(4u + 1u + key_len + 4u + value_len + 4u);
}

static void setup_fixture(void) {
    memset(&g_media, 0, sizeof(g_media));
    g_now = 3000u;
    open_clean_db();
}

static void teardown_fixture(void) {
    close_db_clean();
}

MDB_TEST(phase02_torn_final_append_reports_detail) {
    uint8_t base = 1u;
    uint8_t tail = 2u;
    uint8_t out = 0u;
    lox_db_stats_t stats;

    ASSERT_EQ(lox_kv_set(&g_db, "base", &base, 1u, 0u), LOX_OK);
    g_media.torn_next_program_bytes = 8u;
    ASSERT_EQ(lox_kv_set(&g_db, "tail", &tail, 1u, 0u), LOX_ERR_STORAGE);

    ASSERT_EQ(reopen_after_power_loss(), LOX_OK);
    ASSERT_EQ(lox_kv_get(&g_db, "base", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, base);
    ASSERT_EQ(lox_kv_get(&g_db, "tail", &out, 1u, NULL), LOX_ERR_NOT_FOUND);
    ASSERT_EQ(lox_get_db_stats(&g_db, &stats), LOX_OK);
    ASSERT_EQ(stats.recovery_detail, LOX_RECOVERY_DETAIL_TORN_FINAL_APPEND);
}

MDB_TEST(phase02_discarded_uncommitted_txn_reports_detail) {
    uint8_t payload[64];
    uint16_t payload_len = 0u;
    uint32_t txid = 0x13572468u;
    lox_db_stats_t stats;

    build_txn_set_payload(payload, txid, "ghost", (const uint8_t[]){ 0xABu }, 1u, 0u, &payload_len);
    write_raw_wal_entry(0x03u, 0x00u, payload, payload_len);

    ASSERT_EQ(reopen_after_power_loss(), LOX_OK);
    ASSERT_EQ(lox_kv_get(&g_db, "ghost", &(uint8_t){ 0u }, 1u, NULL), LOX_ERR_NOT_FOUND);
    ASSERT_EQ(lox_get_db_stats(&g_db, &stats), LOX_OK);
    ASSERT_EQ(stats.recovery_detail, LOX_RECOVERY_DETAIL_DISCARDED_UNCOMMITTED_TXN);
}

MDB_TEST(phase02_degraded_fallback_reports_detail) {
    uint8_t value = 5u;
    uint8_t zero = 0u;
    lox_db_stats_t stats;
    const lox_core_t *core;

    ASSERT_EQ(lox_kv_set(&g_db, "keep", &value, 1u, 0u), LOX_OK);
    ASSERT_EQ(lox_flush(&g_db), LOX_OK);
    core = lox_core_const(&g_db);
    storage_force_patch(core->layout.super_a_offset, &zero, sizeof(zero));
    storage_force_patch(core->layout.super_b_offset, &zero, sizeof(zero));

    ASSERT_EQ(reopen_after_power_loss(), LOX_OK);
    ASSERT_EQ(lox_kv_get(&g_db, "keep", &value, 1u, NULL), LOX_OK);
    ASSERT_EQ(lox_get_db_stats(&g_db, &stats), LOX_OK);
    ASSERT_EQ(stats.recovery_detail, LOX_RECOVERY_DETAIL_DEGRADED_FALLBACK);
}

MDB_TEST(phase02_unsupported_format_returns_invalid) {
    const lox_core_t *core;
    uint32_t zero = 0u;

    ASSERT_EQ(lox_kv_set(&g_db, "fmt", &(uint8_t){ 7u }, 1u, 0u), LOX_OK);
    ASSERT_EQ(lox_flush(&g_db), LOX_OK);
    core = lox_core_const(&g_db);
    storage_force_patch(core->layout.bank_a_offset + 4u, &zero, sizeof(zero));
    storage_force_patch(core->layout.bank_b_offset + 4u, &zero, sizeof(zero));

    ASSERT_EQ(reopen_after_power_loss(), LOX_ERR_INVALID);
}

MDB_TEST(phase02_txn_identity_mismatch_is_rejected) {
    uint8_t value = 9u;
    uint32_t entry_offset = 0u;
    uint8_t payload[8];
    uint8_t header[16];
    uint32_t crc;

    ASSERT_EQ(lox_txn_begin(&g_db), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "ident", &value, 1u), LOX_OK);
    ASSERT_EQ(lox_txn_commit(&g_db), LOX_OK);

    ASSERT_EQ(find_wal_entry(0xFFu, 0x05u, &entry_offset), true);
    ASSERT_EQ(g_storage.read(g_storage.ctx, entry_offset, header, sizeof(header)), LOX_OK);
    ASSERT_EQ(g_storage.read(g_storage.ctx, entry_offset + 16u, payload, sizeof(payload)), LOX_OK);
    memset(payload + 0u, 0, sizeof(uint32_t));
    crc = LOX_CRC32(header, 12u);
    crc = lox_crc32(crc, payload, sizeof(payload));
    memcpy(header + 12u, &crc, sizeof(crc));
    storage_force_patch(entry_offset, header, sizeof(header));
    storage_force_patch(entry_offset + 16u, payload, sizeof(payload));

    ASSERT_EQ(reopen_after_power_loss(), LOX_ERR_CORRUPT);
}

int main(void) {
    MDB_RUN_TEST(setup_fixture, teardown_fixture, phase02_torn_final_append_reports_detail);
    MDB_RUN_TEST(setup_fixture, teardown_fixture, phase02_discarded_uncommitted_txn_reports_detail);
    MDB_RUN_TEST(setup_fixture, teardown_fixture, phase02_degraded_fallback_reports_detail);
    MDB_RUN_TEST(setup_fixture, teardown_fixture, phase02_unsupported_format_returns_invalid);
    MDB_RUN_TEST(setup_fixture, teardown_fixture, phase02_txn_identity_mismatch_is_rejected);
    return MDB_RESULT();
}
