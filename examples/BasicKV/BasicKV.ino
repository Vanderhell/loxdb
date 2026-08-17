// SPDX-License-Identifier: MIT
#include <lox.h>

static lox_t db;

void setup() {
    uint32_t value = 42u;
    uint32_t output = 0u;
    size_t output_len = 0u;
    lox_cfg_t cfg = {};
    lox_err_t rc;

    Serial.begin(115200);

    cfg.ram_kb = 32u;
    rc = lox_init(&db, &cfg);
    if (rc != LOX_OK) {
        Serial.printf("lox_init failed: %s\n", lox_err_to_string(rc));
        return;
    }

    rc = lox_kv_put(&db, "answer", &value, sizeof(value));
    if (rc != LOX_OK) {
        Serial.printf("put failed: %s\n", lox_err_to_string(rc));
        return;
    }

    rc = lox_kv_get(&db, "answer", &output, sizeof(output), &output_len);
    if (rc == LOX_OK) {
        Serial.printf("value = %u\n", (unsigned)output);
    } else {
        Serial.printf("get failed: %s\n", lox_err_to_string(rc));
    }
}

void loop() {
}
