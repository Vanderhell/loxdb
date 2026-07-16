// SPDX-License-Identifier: MIT
#include "lox.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    lox_t db;
    lox_cfg_t cfg;
    uint8_t input = 0x33u;
    uint8_t output = 0u;
    size_t out_len = 0u;

    memset(&db, 0, sizeof(db));
    memset(&cfg, 0, sizeof(cfg));
    cfg.storage = NULL;
    cfg.ram_kb = 32u;
    if (lox_init(&db, &cfg) != LOX_OK) {
        fprintf(stderr, "consumer-c: lox_init failed\n");
        return 2;
    }
    if (lox_kv_put(&db, "pkg", &input, sizeof(input)) != LOX_OK) {
        fprintf(stderr, "consumer-c: lox_kv_put failed\n");
        (void)lox_deinit(&db);
        return 3;
    }
    if (lox_kv_get(&db, "pkg", &output, sizeof(output), &out_len) != LOX_OK) {
        fprintf(stderr, "consumer-c: lox_kv_get failed\n");
        (void)lox_deinit(&db);
        return 4;
    }
    if (out_len != sizeof(output) || output != input) {
        fprintf(stderr, "consumer-c: value mismatch\n");
        (void)lox_deinit(&db);
        return 5;
    }
    if (lox_deinit(&db) != LOX_OK) {
        fprintf(stderr, "consumer-c: lox_deinit failed\n");
        return 6;
    }
    return 0;
}
