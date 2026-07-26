// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox.h"
#include "lox_port_ram.h"

#include <stddef.h>
#include <string.h>

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *ptr, size_t size);

static uint32_t g_allocations;

void *__wrap_malloc(size_t size) {
    g_allocations++;
    return __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size) {
    g_allocations++;
    return __real_calloc(count, size);
}

void *__wrap_realloc(void *ptr, size_t size) {
    g_allocations++;
    return __real_realloc(ptr, size);
}

static lox_timestamp_t mock_now(void) {
    return 1000;
}

static void noop(void) {
}

MDB_TEST(normal_kv_operations_allocate_nothing_after_init) {
    lox_t db;
    lox_storage_t storage;
    lox_cfg_t cfg;
    uint8_t value = 0x5Au;
    uint8_t out = 0u;

    memset(&db, 0, sizeof(db));
    memset(&storage, 0, sizeof(storage));
    memset(&cfg, 0, sizeof(cfg));
    ASSERT_EQ(lox_port_ram_init(&storage, 65536u), LOX_OK);
    cfg.storage = &storage;
    cfg.ram_kb = 32u;
    cfg.now = mock_now;
    ASSERT_EQ(lox_init(&db, &cfg), LOX_OK);

    g_allocations = 0u;
    ASSERT_EQ(lox_kv_set(&db, "noalloc", &value, 1u, 10u), LOX_OK);
    ASSERT_EQ(lox_kv_get(&db, "noalloc", &out, 1u, NULL), LOX_OK);
    ASSERT_EQ(out, value);
    ASSERT_EQ(g_allocations, 0u);

    ASSERT_EQ(lox_deinit(&db), LOX_OK);
    lox_port_ram_deinit(&storage);
}

int main(void) {
    MDB_RUN_TEST(noop, noop, normal_kv_operations_allocate_nothing_after_init);
    return MDB_RESULT();
}
