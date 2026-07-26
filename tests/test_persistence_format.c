// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "lox_crc.h"
#include "lox_internal.h"
#include "lox_port_ram.h"

#include <stdlib.h>
#include <string.h>

enum {
    WAL_MAGIC = 0x4D44424Cu,
    WAL_VERSION_LEGACY = 0x00010000u,
    WAL_VERSION_CURRENT = 0x00020000u,
    SNAPSHOT_VERSION_LEGACY = 0x00020000u,
    SNAPSHOT_VERSION_CURRENT = 0x00030000u,
    WAL_ENTRY_MAGIC = 0x454E5452u,
    KV_PAGE_MAGIC = 0x4B565047u,
    TS_PAGE_MAGIC = 0x54535047u,
    REL_PAGE_MAGIC = 0x524C5047u,
    SUPER_MAGIC = 0x53555052u
};

static lox_t g_db;
static lox_storage_t g_storage;
static lox_port_ram_ctx_t *g_ram;
static lox_timestamp_t g_now = 1000;

static lox_timestamp_t mock_now(void) {
    return g_now;
}

static void put_u16(uint8_t *dst, uint16_t value) {
    memcpy(dst, &value, sizeof(value));
}

static void put_u32(uint8_t *dst, uint32_t value) {
    memcpy(dst, &value, sizeof(value));
}

static uint32_t get_u32(const uint8_t *src) {
    uint32_t value;
    memcpy(&value, src, sizeof(value));
    return value;
}

static void put_u64(uint8_t *dst, uint64_t value) {
    put_u32(dst, (uint32_t)value);
    put_u32(dst + 4u, (uint32_t)(value >> 32u));
}

static uint32_t align_u32(uint32_t value, uint32_t align) {
    return ((value + align - 1u) / align) * align;
}

static void open_storage(void) {
    lox_cfg_t cfg;
    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    ASSERT_EQ(lox_port_ram_init(&g_storage, 65536u), LOX_OK);
    g_ram = (lox_port_ram_ctx_t *)g_storage.ctx;
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    cfg.now = mock_now;
    ASSERT_EQ(lox_init(&g_db, &cfg), LOX_OK);
}

static void close_storage(void) {
    if (lox_core_const(&g_db)->magic == LOX_MAGIC) {
        (void)lox_deinit(&g_db);
    }
    lox_port_ram_deinit(&g_storage);
    memset(&g_db, 0, sizeof(g_db));
}

static void crash_drop_db(void) {
    free(lox_core(&g_db)->heap);
    memset(&g_db, 0, sizeof(g_db));
}

static lox_storage_layout_t compute_layout(uint32_t expiration_size) {
    const lox_core_t *core = lox_core_const(&g_db);
    lox_storage_layout_t layout;
    uint32_t erase_size = g_storage.erase_size;
    uint32_t max_entries = LOX_KV_MAX_KEYS - LOX_TXN_STAGE_KEYS;
    uint32_t per_entry =
        1u + (LOX_KV_KEY_MAX_LEN - 1u) + 4u + LOX_KV_VAL_MAX_LEN + expiration_size;
    uint32_t fixed;
    uint32_t max_wal;

    memset(&layout, 0, sizeof(layout));
    layout.super_size = erase_size;
    layout.kv_size = align_u32(max_entries * per_entry + LOX_PAGE_HEADER_SIZE, erase_size);
    layout.ts_size = align_u32((uint32_t)core->ts_arena.capacity + LOX_PAGE_HEADER_SIZE, erase_size);
    layout.rel_size = align_u32((uint32_t)core->rel_arena.capacity + LOX_PAGE_HEADER_SIZE, erase_size);
    layout.bank_size = layout.kv_size + layout.ts_size + layout.rel_size;
    fixed = (layout.super_size * 2u) + (layout.bank_size * 2u);
    max_wal = ((g_storage.capacity - fixed) / erase_size) * erase_size;
    layout.wal_size = erase_size * 8u;
    if (layout.wal_size > max_wal) {
        layout.wal_size = max_wal;
    }
    layout.super_a_offset = layout.wal_size;
    layout.super_b_offset = layout.super_a_offset + layout.super_size;
    layout.bank_a_offset = layout.super_b_offset + layout.super_size;
    layout.bank_b_offset = layout.bank_a_offset + layout.bank_size;
    layout.total_size = layout.bank_b_offset + layout.bank_size;
    return layout;
}

static void write_page(uint32_t offset,
                       uint32_t magic,
                       uint32_t version,
                       const uint8_t *payload,
                       uint32_t payload_len,
                       uint32_t entry_count) {
    uint8_t header[LOX_PAGE_HEADER_SIZE];
    uint32_t payload_crc = 0xFFFFFFFFu;
    memset(header, 0, sizeof(header));
    if (payload_len != 0u) {
        payload_crc = LOX_CRC32(payload, payload_len);
        memcpy(g_ram->buf + offset + LOX_PAGE_HEADER_SIZE, payload, payload_len);
    }
    put_u32(header, magic);
    put_u32(header + 4u, version);
    put_u32(header + 8u, 1u);
    put_u32(header + 12u, payload_len);
    put_u32(header + 16u, entry_count);
    put_u32(header + 20u, payload_crc);
    put_u32(header + 24u, LOX_CRC32(header, 24u));
    memcpy(g_ram->buf + offset, header, sizeof(header));
}

static void write_super(uint32_t offset, uint32_t snapshot_version, uint32_t wal_version) {
    uint8_t super[LOX_SUPERBLOCK_SIZE];
    memset(super, 0, sizeof(super));
    put_u32(super, SUPER_MAGIC);
    put_u32(super + 4u, snapshot_version);
    put_u32(super + 8u, wal_version);
    put_u32(super + 12u, 1u);
    put_u32(super + 16u, 0u);
    put_u32(super + 20u, LOX_CRC32(super, 20u));
    memcpy(g_ram->buf + offset, super, sizeof(super));
}

static void write_wal_header(uint32_t version, uint32_t entry_count) {
    uint8_t header[LOX_WAL_HEADER_SIZE];
    memset(header, 0, sizeof(header));
    put_u32(header, WAL_MAGIC);
    put_u32(header + 4u, version);
    put_u32(header + 8u, entry_count);
    put_u32(header + 12u, 1u);
    put_u32(header + 16u, LOX_CRC32(header, 16u));
    memcpy(g_ram->buf, header, sizeof(header));
}

static uint32_t write_legacy_wal_set(uint32_t offset,
                                     uint32_t sequence,
                                     const char *key,
                                     uint8_t value) {
    uint8_t entry[64];
    uint8_t *payload = entry + 16u;
    uint32_t key_len = (uint32_t)strlen(key);
    uint16_t payload_len = (uint16_t)(1u + key_len + 4u + 1u + 4u);
    uint32_t aligned_len = align_u32(payload_len, 4u);
    uint32_t crc;

    memset(entry, 0, sizeof(entry));
    payload[0] = (uint8_t)key_len;
    memcpy(payload + 1u, key, key_len);
    put_u32(payload + 1u + key_len, 1u);
    payload[1u + key_len + 4u] = value;
    put_u32(payload + 1u + key_len + 5u, 0u);
    put_u32(entry, WAL_ENTRY_MAGIC);
    put_u32(entry + 4u, sequence);
    entry[8] = 0u;
    entry[9] = 0u;
    put_u16(entry + 10u, payload_len);
    crc = LOX_CRC32(entry, 12u);
    crc = lox_crc32(crc, payload, payload_len);
    put_u32(entry + 12u, crc);
    memcpy(g_ram->buf + offset, entry, 16u + aligned_len);
    return offset + 16u + aligned_len;
}

static void construct_media(uint32_t snapshot_version,
                            uint32_t wal_version,
                            uint32_t expiration_size,
                            uint64_t expiration,
                            bool include_legacy_wal) {
    lox_storage_layout_t layout = compute_layout(expiration_size);
    uint8_t payload[64];
    const char *key = "snapshot";
    uint32_t key_len = (uint32_t)strlen(key);
    uint32_t payload_len = 1u + key_len + 4u + 1u + expiration_size;

    memset(g_ram->buf, 0xFF, g_ram->capacity);
    memset(payload, 0, sizeof(payload));
    payload[0] = (uint8_t)key_len;
    memcpy(payload + 1u, key, key_len);
    put_u32(payload + 1u + key_len, 1u);
    payload[1u + key_len + 4u] = 0x2Au;
    if (expiration_size == 4u) {
        put_u32(payload + 1u + key_len + 5u, (uint32_t)expiration);
    } else {
        put_u64(payload + 1u + key_len + 5u, expiration);
    }
    write_page(layout.bank_a_offset, KV_PAGE_MAGIC, snapshot_version, payload, payload_len, 1u);
    write_page(layout.bank_a_offset + layout.kv_size, TS_PAGE_MAGIC, snapshot_version, NULL, 0u, 0u);
    write_page(layout.bank_a_offset + layout.kv_size + layout.ts_size,
               REL_PAGE_MAGIC,
               snapshot_version,
               NULL,
               0u,
               0u);
    write_super(layout.super_a_offset, snapshot_version, wal_version);
    write_wal_header(wal_version, include_legacy_wal ? 1u : 0u);
    if (include_legacy_wal) {
        (void)write_legacy_wal_set(g_storage.erase_size, 1u, "wal", 0x63u);
    }
}

static lox_err_t reopen_existing(void) {
    lox_cfg_t cfg;
    crash_drop_db();
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    cfg.now = mock_now;
    return lox_init(&g_db, &cfg);
}

MDB_TEST(old_snapshot_and_wal_are_opened_and_upgraded) {
    uint8_t value = 0u;
    lox_storage_layout_t current_layout;

    construct_media(
        SNAPSHOT_VERSION_LEGACY, WAL_VERSION_LEGACY, 4u, 0u, true);
    ASSERT_EQ(reopen_existing(), LOX_OK);
    ASSERT_EQ(lox_kv_get(&g_db, "snapshot", &value, 1u, NULL), LOX_OK);
    ASSERT_EQ(value, 0x2Au);
    ASSERT_EQ(lox_kv_get(&g_db, "wal", &value, 1u, NULL), LOX_OK);
    ASSERT_EQ(value, 0x63u);

    current_layout = lox_core_const(&g_db)->layout;
    ASSERT_EQ(get_u32(g_ram->buf + current_layout.super_b_offset + 4u), SNAPSHOT_VERSION_CURRENT);
    ASSERT_EQ(get_u32(g_ram->buf + current_layout.super_b_offset + 8u), WAL_VERSION_CURRENT);
    ASSERT_EQ(get_u32(g_ram->buf + 4u), WAL_VERSION_CURRENT);
}

MDB_TEST(expiration_too_wide_for_timestamp_type_fails) {
    construct_media(
        SNAPSHOT_VERSION_CURRENT, WAL_VERSION_CURRENT, 8u, UINT64_MAX, false);
    ASSERT_EQ(reopen_existing(), LOX_ERR_OVERFLOW);
}

int main(void) {
    MDB_RUN_TEST(open_storage, close_storage, old_snapshot_and_wal_are_opened_and_upgraded);
    MDB_RUN_TEST(open_storage, close_storage, expiration_too_wide_for_timestamp_type_fails);
    return MDB_RESULT();
}
