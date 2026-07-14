// SPDX-License-Identifier: MIT
#include "lox_config.h"
#include "lox.h"

#include <stddef.h>
#include <stdint.h>

static void strict_c99_gate_touch(void) {
    typedef struct {
        char pad;
        lox_t value;
    } strict_c99_gate_handle_holder_t;
    lox_t handle;
    lox_schema_t schema;

    (void)sizeof(handle);
    (void)sizeof(schema);
    (void)offsetof(strict_c99_gate_handle_holder_t, value);
    (void)LOX_CONFIG_FINGERPRINT;
}

int main(void) {
    strict_c99_gate_touch();
    return (lox_config_fingerprint() == (uint32_t)LOX_CONFIG_FINGERPRINT) ? 0 : 1;
}
