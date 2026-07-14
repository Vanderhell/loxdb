// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox_config.h"
#include "lox.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LOX_CT_ASSERT(name, expr) typedef char lox_ct_assert_##name[(expr) ? 1 : -1]

typedef struct {
    char pad;
    lox_t handle;
} lox_handle_holder_t;

typedef struct {
    char pad;
    lox_schema_t schema;
} lox_schema_holder_t;

#pragma pack(push, 1)
typedef struct {
    char pad;
    lox_t handle;
} packed_lox_handle_holder_t;

typedef struct {
    char pad;
    lox_schema_t schema;
} packed_lox_schema_holder_t;
#pragma pack(pop)

static lox_t g_global_handle;
static lox_t g_global_handles[2];
static lox_schema_t g_global_schema;
static lox_schema_t g_global_schemas[2];

LOX_CT_ASSERT(handle_global_alignment, offsetof(lox_handle_holder_t, handle) % sizeof(long double) == 0u);
LOX_CT_ASSERT(schema_global_alignment, offsetof(lox_schema_holder_t, schema) % sizeof(long double) == 0u);
LOX_CT_ASSERT(packed_handle_offset, offsetof(packed_lox_handle_holder_t, handle) == 1u);
LOX_CT_ASSERT(packed_schema_offset, offsetof(packed_lox_schema_holder_t, schema) == 1u);

static void init_cfg(lox_cfg_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->ram_kb = 32u;
}

static void init_and_deinit_handle(lox_t *handle) {
    lox_cfg_t cfg;

    init_cfg(&cfg);
    ASSERT_EQ(lox_init(handle, &cfg), LOX_OK);
    ASSERT_EQ(lox_config_fingerprint(), (uint32_t)LOX_CONFIG_FINGERPRINT);
    ASSERT_EQ(lox_deinit(handle), LOX_OK);
}

static void setup_noop(void) {
}

static void teardown_noop(void) {
}

MDB_TEST(public_handle_stack_alignment_ok) {
    lox_t handle;

    memset(&handle, 0, sizeof(handle));
    init_and_deinit_handle(&handle);
}

MDB_TEST(public_handle_global_alignment_ok) {
    memset(&g_global_handle, 0, sizeof(g_global_handle));
    init_and_deinit_handle(&g_global_handle);
}

MDB_TEST(public_handle_array_alignment_ok) {
    lox_cfg_t cfg;

    memset(&g_global_handles, 0, sizeof(g_global_handles));
    init_cfg(&cfg);
    ASSERT_EQ(lox_init(&g_global_handles[0], &cfg), LOX_OK);
    ASSERT_EQ(lox_deinit(&g_global_handles[0]), LOX_OK);
    ASSERT_EQ(lox_init(&g_global_handles[1], &cfg), LOX_OK);
    ASSERT_EQ(lox_deinit(&g_global_handles[1]), LOX_OK);
}

MDB_TEST(public_schema_stack_alignment_ok) {
    lox_schema_t schema;

    memset(&schema, 0, sizeof(schema));
    ASSERT_EQ(lox_schema_init(&schema, "s", 1u), LOX_OK);
}

MDB_TEST(public_schema_global_alignment_ok) {
    memset(&g_global_schema, 0, sizeof(g_global_schema));
    ASSERT_EQ(lox_schema_init(&g_global_schema, "g", 1u), LOX_OK);
}

MDB_TEST(public_schema_array_alignment_ok) {
    memset(&g_global_schemas, 0, sizeof(g_global_schemas));
    ASSERT_EQ(lox_schema_init(&g_global_schemas[0], "a", 1u), LOX_OK);
    ASSERT_EQ(lox_schema_init(&g_global_schemas[1], "b", 1u), LOX_OK);
}

MDB_TEST(public_packed_wrapper_layout_recorded) {
    packed_lox_handle_holder_t packed_handle;
    packed_lox_schema_holder_t packed_schema;

    ASSERT_EQ(offsetof(packed_lox_handle_holder_t, handle), 1u);
    ASSERT_EQ(offsetof(packed_lox_schema_holder_t, schema), 1u);
    memset(&packed_handle, 0, sizeof(packed_handle));
    memset(&packed_schema, 0, sizeof(packed_schema));
    ASSERT_EQ(sizeof(packed_handle) > sizeof(lox_t), 1);
    ASSERT_EQ(sizeof(packed_schema) > sizeof(lox_schema_t), 1);
}

int main(void) {
    MDB_RUN_TEST(setup_noop, teardown_noop, public_handle_stack_alignment_ok);
    MDB_RUN_TEST(setup_noop, teardown_noop, public_handle_global_alignment_ok);
    MDB_RUN_TEST(setup_noop, teardown_noop, public_handle_array_alignment_ok);
    MDB_RUN_TEST(setup_noop, teardown_noop, public_schema_stack_alignment_ok);
    MDB_RUN_TEST(setup_noop, teardown_noop, public_schema_global_alignment_ok);
    MDB_RUN_TEST(setup_noop, teardown_noop, public_schema_array_alignment_ok);
    MDB_RUN_TEST(setup_noop, teardown_noop, public_packed_wrapper_layout_recorded);
    return MDB_RESULT();
}
