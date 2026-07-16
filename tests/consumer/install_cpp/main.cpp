// SPDX-License-Identifier: MIT
#include "lox_cpp.hpp"

#include <cstdio>
#include <cstring>

int main() {
    loxdb::cpp::Database db;
    uint8_t input = 0x44u;
    uint8_t output = 0u;
    size_t out_len = 0u;
    lox_cfg_t cfg = {};

    cfg.storage = NULL;
    cfg.ram_kb = 32u;
    if (db.init(cfg) != LOX_OK) {
        std::fprintf(stderr, "consumer-cpp: lox_init failed\n");
        return 2;
    }
    if (db.kv_put("pkg", &input, sizeof(input)) != LOX_OK) {
        std::fprintf(stderr, "consumer-cpp: lox_kv_put failed\n");
        (void)db.deinit();
        return 3;
    }
    if (db.kv_get("pkg", &output, sizeof(output), &out_len) != LOX_OK) {
        std::fprintf(stderr, "consumer-cpp: lox_kv_get failed\n");
        (void)db.deinit();
        return 4;
    }
    if (out_len != sizeof(output) || output != input) {
        std::fprintf(stderr, "consumer-cpp: value mismatch\n");
        (void)db.deinit();
        return 5;
    }
    if (db.deinit() != LOX_OK) {
        std::fprintf(stderr, "consumer-cpp: lox_deinit failed\n");
        return 6;
    }
    return 0;
}
