// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <lox.h>

static lox_t db;

void setup() {
    lox_cfg_t cfg = {};
    cfg.ram_kb = 32u;
    (void)lox_init(&db, &cfg);
}

void loop() {
}
