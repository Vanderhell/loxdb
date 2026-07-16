// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox_config.h"
#include "lox.h"

#include <stddef.h>
#include <stdint.h>

uint32_t public_config_identity_tu_a_fingerprint(void);
uint32_t public_config_identity_tu_b_fingerprint(void);
size_t public_config_identity_tu_a_handle_size(void);
size_t public_config_identity_tu_b_schema_size(void);
int public_config_identity_tu_a_stack_roundtrip(void);
int public_config_identity_tu_b_schema_roundtrip(void);

MDB_TEST(public_config_identity_multi_tu_matches) {
    ASSERT_EQ(public_config_identity_tu_a_fingerprint(), (uint32_t)LOX_CONFIG_FINGERPRINT);
    ASSERT_EQ(public_config_identity_tu_b_fingerprint(), (uint32_t)LOX_CONFIG_FINGERPRINT);
    ASSERT_GT(public_config_identity_tu_a_handle_size(), 0u);
    ASSERT_GT(public_config_identity_tu_b_schema_size(), 0u);
    ASSERT_EQ(public_config_identity_tu_a_stack_roundtrip(), 1);
    ASSERT_EQ(public_config_identity_tu_b_schema_roundtrip(), 1);
}

static void setup_noop(void) {
}

static void teardown_noop(void) {
}

int main(void) {
    MDB_RUN_TEST(setup_noop, teardown_noop, public_config_identity_multi_tu_matches);
    return MDB_RESULT();
}
