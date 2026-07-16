// SPDX-License-Identifier: MIT
#include "lox_config.h"
#include "lox.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint32_t public_config_identity_tu_a_fingerprint(void) {
    return lox_config_fingerprint();
}

size_t public_config_identity_tu_a_handle_size(void) {
    return sizeof(lox_t);
}

int public_config_identity_tu_a_stack_roundtrip(void) {
    lox_t handle;
    lox_cfg_t cfg;

    memset(&handle, 0, sizeof(handle));
    memset(&cfg, 0, sizeof(cfg));
    cfg.ram_kb = 32u;
    return (lox_init(&handle, &cfg) == LOX_OK && lox_deinit(&handle) == LOX_OK) ? 1 : 0;
}
