// SPDX-License-Identifier: MIT
#include "lox_config.h"
#include "lox.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint32_t public_config_identity_tu_b_fingerprint(void) {
    return lox_config_fingerprint();
}

size_t public_config_identity_tu_b_schema_size(void) {
    return sizeof(lox_schema_t);
}

int public_config_identity_tu_b_schema_roundtrip(void) {
    lox_schema_t schema;

    memset(&schema, 0, sizeof(schema));
    return (lox_schema_init(&schema, "tu_b", 1u) == LOX_OK) ? 1 : 0;
}
