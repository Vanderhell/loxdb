// SPDX-License-Identifier: MIT
#define LOX_ENABLE_WAL 0
#include "microtest.h"
#include "lox.h"
#include "../src/lox_internal.h"

#include <stdlib.h>
#include <string.h>

enum {
    MEDIA_CAPACITY = 262144u,
    ERASE_SIZE = 4096u,
    WAL_MAGIC = 0x4D44424Cu
};

typedef struct {
    uint8_t bytes[MEDIA_CAPACITY];
    uint32_t writes;
    uint32_t erases;
    uint32_t reads;
    uint32_t wal_magic_writes;
    uint32_t invalid_erases;
    uint32_t fail_write;
    lox_storage_layout_t layout;
} no_wal_media_t;

static no_wal_media_t g_media;
static lox_storage_t g_storage;
static lox_t g_db;

static bool erase_belongs_to_snapshot_layout(uint32_t offset) {
    const lox_storage_layout_t *layout = &g_media.layout;
    if (offset == layout->super_a_offset || offset == layout->super_b_offset) {
        return true;
    }
    if (offset >= layout->bank_a_offset && offset < layout->bank_a_offset + layout->bank_size) {
        return true;
    }
    return offset >= layout->bank_b_offset && offset < layout->bank_b_offset + layout->bank_size;
}

static lox_err_t media_read(void *ctx, uint32_t offset, void *buf, size_t len) {
    no_wal_media_t *media = (no_wal_media_t *)ctx;
    if (media == NULL || buf == NULL || offset > MEDIA_CAPACITY ||
        len > MEDIA_CAPACITY - offset) {
        return LOX_ERR_INVALID;
    }
    media->reads++;
    memcpy(buf, media->bytes + offset, len);
    return LOX_OK;
}

static lox_err_t media_write(void *ctx, uint32_t offset, const void *buf, size_t len) {
    no_wal_media_t *media = (no_wal_media_t *)ctx;
    uint32_t magic = 0u;
    if (media == NULL || buf == NULL || offset > MEDIA_CAPACITY ||
        len > MEDIA_CAPACITY - offset) {
        return LOX_ERR_INVALID;
    }
    media->writes++;
    if (len >= sizeof(magic)) {
        memcpy(&magic, buf, sizeof(magic));
        if (magic == WAL_MAGIC) {
            media->wal_magic_writes++;
        }
    }
    if (media->fail_write != 0u && media->writes == media->fail_write) {
        return LOX_ERR_STORAGE;
    }
    memcpy(media->bytes + offset, buf, len);
    return LOX_OK;
}

static lox_err_t media_erase(void *ctx, uint32_t offset) {
    no_wal_media_t *media = (no_wal_media_t *)ctx;
    if (media == NULL || offset > MEDIA_CAPACITY || ERASE_SIZE > MEDIA_CAPACITY - offset) {
        return LOX_ERR_INVALID;
    }
    media->erases++;
    if (!erase_belongs_to_snapshot_layout(offset)) {
        media->invalid_erases++;
    }
    memset(media->bytes + offset, 0xFF, ERASE_SIZE);
    return LOX_OK;
}

static lox_err_t media_sync(void *ctx) {
    return ctx != NULL ? LOX_OK : LOX_ERR_INVALID;
}

static void bind_storage(uint32_t capacity) {
    memset(&g_storage, 0, sizeof(g_storage));
    g_storage.ctx = &g_media;
    g_storage.read = media_read;
    g_storage.write = media_write;
    g_storage.erase = media_erase;
    g_storage.sync = media_sync;
    g_storage.capacity = capacity;
    g_storage.erase_size = ERASE_SIZE;
    g_storage.write_size = 1u;
}

static lox_cfg_t make_cfg(void) {
    lox_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = &g_storage;
    cfg.ram_kb = 32u;
    return cfg;
}

static void prepare_virgin_media(lox_preflight_report_t *report) {
    lox_cfg_t cfg;

    memset(&g_media, 0, sizeof(g_media));
    memset(g_media.bytes, 0xFF, sizeof(g_media.bytes));
    bind_storage(MEDIA_CAPACITY);
    cfg = make_cfg();
    ASSERT_EQ(lox_preflight(&cfg, report), LOX_OK);
    g_media.layout.wal_offset = report->wal_offset;
    g_media.layout.wal_size = report->wal_size;
    g_media.layout.super_a_offset = report->super_a_offset;
    g_media.layout.super_b_offset = report->super_b_offset;
    g_media.layout.super_size = report->superblock_bytes;
    g_media.layout.bank_a_offset = report->bank_a_offset;
    g_media.layout.bank_b_offset = report->bank_b_offset;
    g_media.layout.bank_size = report->bank_size;
}

static lox_err_t open_db(void) {
    lox_cfg_t cfg = make_cfg();
    memset(&g_db, 0, sizeof(g_db));
    return lox_init(&g_db, &cfg);
}

static void crash_drop_db(void) {
    free(lox_core(&g_db)->heap);
    memset(&g_db, 0, sizeof(g_db));
}

static void noop(void) {
}

static void create_no_wal_table(lox_table_t **out_table) {
    lox_schema_t schema;

    ASSERT_EQ(lox_schema_init(&schema, "records", 8u), LOX_OK);
    ASSERT_EQ(lox_schema_add(&schema, "id", LOX_COL_U32, sizeof(uint32_t), true), LOX_OK);
    ASSERT_EQ(lox_schema_seal(&schema), LOX_OK);
    ASSERT_EQ(lox_table_create(&g_db, &schema), LOX_OK);
    ASSERT_EQ(lox_table_get(&g_db, "records", out_table), LOX_OK);
}

MDB_TEST(no_wal_preflight_and_exact_capacity_match_init) {
    lox_preflight_report_t report;
    lox_cfg_t cfg = make_cfg();
    const lox_storage_layout_t *live;
    uint32_t required;
    uint32_t equivalent_wal_minimum;

    prepare_virgin_media(&report);
    cfg = make_cfg();
    required = report.storage_required_bytes;
    equivalent_wal_minimum = required + (2u * ERASE_SIZE);

    ASSERT_EQ(report.wal_enabled, 0u);
    ASSERT_EQ(report.wal_size, 0u);
    ASSERT_EQ(report.wal_offset, 0u);
    ASSERT_EQ(report.super_a_offset, 0u);
    ASSERT_EQ(report.storage_layout_bytes, required);
    ASSERT_EQ(required % ERASE_SIZE, 0u);
    ASSERT_GT(equivalent_wal_minimum, required);
    ASSERT_LE(equivalent_wal_minimum, MEDIA_CAPACITY);

    g_storage.capacity = required - 1u;
    ASSERT_EQ(lox_preflight(&cfg, &report), LOX_ERR_STORAGE);
    ASSERT_EQ(report.storage_required_bytes, required);
    ASSERT_EQ(open_db(), LOX_ERR_STORAGE);

    g_storage.capacity = required;
    ASSERT_EQ(open_db(), LOX_OK);
    live = &lox_core_const(&g_db)->layout;
    ASSERT_EQ(live->wal_size, 0u);
    ASSERT_EQ(live->wal_offset, 0u);
    ASSERT_EQ(live->super_a_offset, report.super_a_offset);
    ASSERT_EQ(live->super_b_offset, report.super_b_offset);
    ASSERT_EQ(live->bank_a_offset, report.bank_a_offset);
    ASSERT_EQ(live->bank_b_offset, report.bank_b_offset);
    ASSERT_EQ(live->total_size, required);
    ASSERT_EQ(lox_core_const(&g_db)->wal_used, 0u);
    ASSERT_EQ(lox_core_const(&g_db)->wal_entry_count, 0u);
    ASSERT_EQ(g_media.wal_magic_writes, 0u);
    ASSERT_EQ(g_media.invalid_erases, 0u);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
}

MDB_TEST(no_wal_stats_admission_and_snapshot_persistence_are_zero_wal) {
    lox_preflight_report_t report;
    lox_stats_t stats;
    lox_db_stats_t db_stats;
    lox_effective_capacity_t capacity;
    lox_admission_t admission;
    uint8_t value = 0x5Au;
    uint8_t out = 0u;

    prepare_virgin_media(&report);
    g_storage.capacity = report.storage_required_bytes;
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "persist", &value, 1u), LOX_OK);
    ASSERT_EQ(lox_stats(&g_db, &stats), LOX_OK);
    ASSERT_EQ(lox_get_db_stats(&g_db, &db_stats), LOX_OK);
    ASSERT_EQ(lox_get_effective_capacity(&g_db, &capacity), LOX_OK);
    ASSERT_EQ(lox_admit_kv_set(&g_db, "next", 1u, &admission), LOX_OK);
    ASSERT_EQ(stats.wal_bytes_total, 0u);
    ASSERT_EQ(stats.wal_bytes_used, 0u);
    ASSERT_EQ(db_stats.wal_bytes_total, 0u);
    ASSERT_EQ(db_stats.wal_bytes_used, 0u);
    ASSERT_EQ(capacity.wal_budget_total, 0u);
    ASSERT_EQ(capacity.wal_budget_used, 0u);
    ASSERT_EQ(capacity.wal_budget_free, 0u);
    ASSERT_EQ(admission.required_wal_bytes, 0u);
    ASSERT_EQ(admission.wal_bytes_free, 0u);
    ASSERT_EQ(lox_flush(&g_db), LOX_OK);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_kv_get(&g_db, "persist", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, value);
    ASSERT_EQ(g_media.wal_magic_writes, 0u);
    ASSERT_EQ(g_media.invalid_erases, 0u);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
}

MDB_TEST(no_wal_interrupted_snapshot_keeps_previous_bank) {
    lox_preflight_report_t report;
    uint8_t old_value = 1u;
    uint8_t new_value = 2u;
    uint8_t out = 0u;

    prepare_virgin_media(&report);
    g_storage.capacity = report.storage_required_bytes;
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_kv_put(&g_db, "stable", &old_value, 1u), LOX_OK);

    g_media.writes = 0u;
    g_media.fail_write = 1u;
    ASSERT_EQ(lox_kv_put(&g_db, "stable", &new_value, 1u), LOX_ERR_INDETERMINATE);
    crash_drop_db();
    g_media.fail_write = 0u;
    g_media.writes = 0u;

    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_kv_get(&g_db, "stable", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, old_value);
    ASSERT_EQ(g_media.wal_magic_writes, 0u);
    ASSERT_EQ(g_media.invalid_erases, 0u);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
}

MDB_TEST(no_wal_ts_mutations_survive_immediate_crash) {
    lox_preflight_report_t report;
    lox_ts_sample_t sample;
    uint32_t value = 42u;

    prepare_virgin_media(&report);
    g_storage.capacity = report.storage_required_bytes;
    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_ts_register(&g_db, "temp", LOX_TS_U32, 0u), LOX_OK);
    ASSERT_EQ(lox_ts_insert(&g_db, "temp", 100u, &value), LOX_OK);
    crash_drop_db();

    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_ts_last(&g_db, "temp", &sample), LOX_OK);
    ASSERT_EQ(sample.ts, 100u);
    ASSERT_EQ(sample.v.u32, value);
    ASSERT_EQ(lox_ts_clear(&g_db, "temp"), LOX_OK);
    crash_drop_db();

    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_ts_last(&g_db, "temp", &sample), LOX_ERR_NOT_FOUND);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
}

MDB_TEST(no_wal_rel_mutations_survive_immediate_crash) {
    lox_preflight_report_t report;
    lox_table_t *table;
    uint32_t id = 7u;
    uint32_t count = 0u;
    uint32_t deleted = 0u;

    prepare_virgin_media(&report);
    g_storage.capacity = report.storage_required_bytes;
    ASSERT_EQ(open_db(), LOX_OK);
    create_no_wal_table(&table);
    crash_drop_db();

    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_table_get(&g_db, "records", &table), LOX_OK);
    ASSERT_EQ(lox_rel_insert(&g_db, table, &id), LOX_OK);
    crash_drop_db();

    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_table_get(&g_db, "records", &table), LOX_OK);
    ASSERT_EQ(lox_rel_count(table, &count), LOX_OK);
    ASSERT_EQ(count, 1u);
    ASSERT_EQ(lox_rel_delete(&g_db, table, &id, &deleted), LOX_OK);
    ASSERT_EQ(deleted, 1u);
    crash_drop_db();

    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_table_get(&g_db, "records", &table), LOX_OK);
    ASSERT_EQ(lox_rel_count(table, &count), LOX_OK);
    ASSERT_EQ(count, 0u);
    ASSERT_EQ(lox_rel_insert(&g_db, table, &id), LOX_OK);
    ASSERT_EQ(lox_rel_clear(&g_db, table), LOX_OK);
    crash_drop_db();

    ASSERT_EQ(open_db(), LOX_OK);
    ASSERT_EQ(lox_table_get(&g_db, "records", &table), LOX_OK);
    ASSERT_EQ(lox_rel_count(table, &count), LOX_OK);
    ASSERT_EQ(count, 0u);
    ASSERT_EQ(lox_deinit(&g_db), LOX_OK);
}

int main(void) {
    MDB_RUN_TEST(noop, noop, no_wal_preflight_and_exact_capacity_match_init);
    MDB_RUN_TEST(noop, noop, no_wal_stats_admission_and_snapshot_persistence_are_zero_wal);
    MDB_RUN_TEST(noop, noop, no_wal_interrupted_snapshot_keeps_previous_bank);
    MDB_RUN_TEST(noop, noop, no_wal_ts_mutations_survive_immediate_crash);
    MDB_RUN_TEST(noop, noop, no_wal_rel_mutations_survive_immediate_crash);
    return MDB_RESULT();
}
