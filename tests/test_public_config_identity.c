// SPDX-License-Identifier: MIT
#include "microtest.h"
#include "lox_config.h"
#include "lox.h"

#include <stdint.h>
#include <string.h>

MDB_TEST(public_config_identity_matches_header_constants) {
    ASSERT_EQ((uint32_t)LOX_HANDLE_SIZE, (uint32_t)LOX_CONFIG_HANDLE_SIZE);
    ASSERT_EQ((uint32_t)LOX_SCHEMA_SIZE, (uint32_t)LOX_CONFIG_SCHEMA_SIZE);
    ASSERT_EQ((uint32_t)LOX_TS_RAW_MAX, (uint32_t)LOX_CONFIG_TS_RAW_MAX);
    ASSERT_EQ((uint32_t)LOX_REL_INDEX_KEY_MAX, (uint32_t)LOX_CONFIG_REL_INDEX_KEY_MAX);
    ASSERT_EQ((uint32_t)sizeof(LOX_TIMESTAMP_TYPE), (uint32_t)sizeof(LOX_CONFIG_TIMESTAMP_TYPE));
    ASSERT_EQ(lox_config_fingerprint(), (uint32_t)LOX_CONFIG_FINGERPRINT);
}

MDB_TEST(public_config_identity_stays_stable_in_local_build) {
    ASSERT_EQ(LOX_CONFIG_PUBLIC_VERSION, 1u);
    ASSERT_GT((uint32_t)LOX_CONFIG_FINGERPRINT, 0u);
}

static void setup_noop(void) {
}

static void teardown_noop(void) {
}

int main(void) {
    MDB_RUN_TEST(setup_noop, teardown_noop, public_config_identity_matches_header_constants);
    MDB_RUN_TEST(setup_noop, teardown_noop, public_config_identity_stays_stable_in_local_build);
    return MDB_RESULT();
}
