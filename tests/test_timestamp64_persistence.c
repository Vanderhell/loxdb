// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "lox_crc.h"
#include "lox_port_ram.h"
#include "../src/lox_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    WAL_MAGIC = 0x4D44424Cu,
    WAL_VERSION_LEGACY = 0x00010000u,
    SNAPSHOT_VERSION_LEGACY = 0x00020000u,
    KV_PAGE_MAGIC = 0x4B565047u,
    TS_PAGE_MAGIC = 0x54535047u,
    REL_PAGE_MAGIC = 0x524C5047u,
    SUPER_MAGIC = 0x53555052u
};

static lox_t g_db;
static lox_storage_t g_storage;
static lox_port_ram_ctx_t *g_ram;
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

static void crash_drop_db(void) {
    free(lox_core(&g_db)->heap);
    memset(&g_db, 0, sizeof(g_db));
}

static void put_u32(uint8_t *dst, uint32_t value) {
    memcpy(dst, &value, sizeof(value));
}

static void put_u64(uint8_t *dst, uint64_t value) {
    put_u32(dst, (uint32_t)value);
    put_u32(dst + 4u, (uint32_t)(value >> 32u));
}

static void write_page(uint32_t offset,
                       uint32_t magic,
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
    put_u32(header + 4u, SNAPSHOT_VERSION_LEGACY);
    put_u32(header + 8u, 1u);
    put_u32(header + 12u, payload_len);
    put_u32(header + 16u, entry_count);
    put_u32(header + 20u, payload_crc);
    put_u32(header + 24u, LOX_CRC32(header, 24u));
    memcpy(g_ram->buf + offset, header, sizeof(header));
}

static void construct_legacy_ts_snapshot(size_t ts_arena_size,
                                         size_t rel_arena_size,
                                         uint32_t timestamp,
                                         uint32_t value) {
    lox_storage_layout_t layout;
    uint32_t required_size;
    uint8_t payload[64];
    uint8_t super[LOX_SUPERBLOCK_SIZE];
    uint8_t wal[LOX_WAL_HEADER_SIZE];
    const char *name = "legacy";
    uint32_t name_len = (uint32_t)strlen(name);
    uint32_t payload_len = 1u + name_len + 23u;

    ASSERT_EQ(lox_compute_storage_layout(
                  &g_storage, ts_arena_size, rel_arena_size, 4u, &layout, &required_size),
              LOX_OK);
    (void)required_size;
    memset(g_ram->buf, 0xFF, g_ram->capacity);
    payload[0] = (uint8_t)name_len;
    memcpy(payload + 1u, name, name_len);
    payload[1u + name_len] = (uint8_t)LOX_TS_U32;
    put_u32(payload + 1u + name_len + 1u, 0u);
    payload[1u + name_len + 5u] = 0u;
    payload[1u + name_len + 6u] = 0u;
    put_u32(payload + 1u + name_len + 7u, 1u);
    put_u64(payload + 1u + name_len + 11u, timestamp);
    put_u32(payload + 1u + name_len + 19u, value);

    write_page(layout.bank_a_offset, KV_PAGE_MAGIC, NULL, 0u, 0u);
    write_page(layout.bank_a_offset + layout.kv_size, TS_PAGE_MAGIC, payload, payload_len, 1u);
    write_page(layout.bank_a_offset + layout.kv_size + layout.ts_size,
               REL_PAGE_MAGIC,
               NULL,
               0u,
               0u);

    memset(super, 0, sizeof(super));
    put_u32(super, SUPER_MAGIC);
    put_u32(super + 4u, SNAPSHOT_VERSION_LEGACY);
    put_u32(super + 8u, WAL_VERSION_LEGACY);
    put_u32(super + 12u, 1u);
    put_u32(super + 16u, 0u);
    put_u32(super + 20u, LOX_CRC32(super, 20u));
    memcpy(g_ram->buf + layout.super_a_offset, super, sizeof(super));

    memset(wal, 0, sizeof(wal));
    put_u32(wal, WAL_MAGIC);
    put_u32(wal + 4u, WAL_VERSION_LEGACY);
    put_u32(wal + 8u, 0u);
    put_u32(wal + 12u, 1u);
    put_u32(wal + 16u, LOX_CRC32(wal, 16u));
    memcpy(g_ram->buf + layout.wal_offset, wal, sizeof(wal));
}

static bool capture_sample(const lox_ts_sample_t *sample, void *ctx) {
    lox_ts_sample_t *captured = (lox_ts_sample_t *)ctx;
    *captured = *sample;
    return true;
}

static void noop(void) {
}

MDB_TEST(ts_above_u32_survives_wal_and_snapshot_recovery) {
    const uint64_t timestamp = UINT64_C(0x100000000) + 77u;
    uint32_t value = 0xA5A55A5Au;
    lox_ts_sample_t sample;
    size_t count = 0u;

    memset(&g_storage, 0, sizeof(g_storage));
    ASSERT_EQ(lox_port_ram_init(&g_storage, 65536u), LOX_OK);
    g_ram = (lox_port_ram_ctx_t *)g_storage.ctx;
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_ts_register(&g_db, "wide", LOX_TS_U32, 0u), LOX_OK);
    ASSERT_EQ(lox_ts_insert(&g_db, "wide", timestamp, &value), LOX_OK);

    crash_drop_db();
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_ts_last(&g_db, "wide", &sample), LOX_OK);
    ASSERT_EQ(sample.ts, timestamp);
    ASSERT_EQ(sample.v.u32, value);
    ASSERT_EQ(lox_ts_query(&g_db, "wide", timestamp, timestamp, capture_sample, &sample), LOX_OK);
    ASSERT_EQ(sample.ts, timestamp);
    ASSERT_EQ(lox_ts_count(&g_db, "wide", timestamp, timestamp, &count), LOX_OK);
    ASSERT_EQ(count, 1u);
    ASSERT_EQ(lox_flush(&g_db), LOX_OK);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);

    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_ts_last(&g_db, "wide", &sample), LOX_OK);
    ASSERT_EQ(sample.ts, timestamp);
    ASSERT_EQ(sample.v.u32, value);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
    lox_port_ram_deinit(&g_storage);
}

MDB_TEST(legacy_u32_ts_snapshot_opens_in_u64_build) {
    lox_ts_sample_t sample;
    size_t ts_arena_size;
    size_t rel_arena_size;

    memset(&g_storage, 0, sizeof(g_storage));
    ASSERT_EQ(lox_port_ram_init(&g_storage, 65536u), LOX_OK);
    g_ram = (lox_port_ram_ctx_t *)g_storage.ctx;
    ASSERT_EQ(open_db(), LOX_OK);
    ts_arena_size = lox_core_const(&g_db)->ts_arena.capacity;
    rel_arena_size = lox_core_const(&g_db)->rel_arena.capacity;
    crash_drop_db();
    construct_legacy_ts_snapshot(
        ts_arena_size, rel_arena_size, UINT32_MAX, 0x55AA55AAu);
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_ts_last(&g_db, "legacy", &sample), LOX_OK);
    ASSERT_EQ(sample.ts, UINT32_MAX);
    ASSERT_EQ(sample.v.u32, 0x55AA55AAu);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
    lox_port_ram_deinit(&g_storage);
}

int main(void) {
    MDB_RUN_TEST(noop, noop, ts_above_u32_survives_wal_and_snapshot_recovery);
    MDB_RUN_TEST(noop, noop, legacy_u32_ts_snapshot_opens_in_u64_build);
    return MDB_RESULT();
}
