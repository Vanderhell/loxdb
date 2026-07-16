// SPDX-License-Identifier: MIT
#include "lox_config.h"
#include "lox.h"

#include <stddef.h>

typedef struct {
    char pad;
    lox_t handle;
} ubsan_handle_holder_t;

typedef struct {
    char pad;
    lox_schema_t schema;
} ubsan_schema_holder_t;

#pragma pack(push, 1)
typedef struct {
    char pad;
    lox_t handle;
} ubsan_packed_handle_holder_t;

typedef struct {
    char pad;
    lox_schema_t schema;
} ubsan_packed_schema_holder_t;
#pragma pack(pop)

static lox_t g_handle;
static lox_t g_handles[2];
static lox_schema_t g_schema;
static lox_schema_t g_schemas[2];

_Static_assert(offsetof(ubsan_handle_holder_t, handle) % _Alignof(lox_t) == 0u, "stack wrapper must preserve lox_t alignment");
_Static_assert(offsetof(ubsan_schema_holder_t, schema) % _Alignof(lox_schema_t) == 0u, "stack wrapper must preserve lox_schema_t alignment");
_Static_assert(offsetof(ubsan_packed_handle_holder_t, handle) == 1u, "packed wrapper must remain packed");
_Static_assert(offsetof(ubsan_packed_schema_holder_t, schema) == 1u, "packed wrapper must remain packed");

int main(void) {
    ubsan_handle_holder_t handle_holder = {0};
    ubsan_schema_holder_t schema_holder = {0};
    ubsan_packed_handle_holder_t packed_handle_holder = {0};
    ubsan_packed_schema_holder_t packed_schema_holder = {0};

    (void)g_handle;
    (void)g_handles;
    (void)g_schema;
    (void)g_schemas;
    (void)handle_holder;
    (void)schema_holder;
    (void)packed_handle_holder;
    (void)packed_schema_holder;
    return 0;
}
