// SPDX-License-Identifier: MIT
#include "lox.h"
#include "lox_port_posix.h"
#include "../src/lox_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !LOX_ENABLE_KV || LOX_ENABLE_TS || LOX_ENABLE_REL || !LOX_ENABLE_WAL
#error "FOOTPRINT_MIN engine and durability contract mismatch"
#endif
#if LOX_RAM_KB != 8u
#error "FOOTPRINT_MIN RAM contract mismatch"
#endif

int main(void) {
    lox_storage_t storage;
    lox_cfg_t cfg;
    lox_t db;
    uint8_t in = 0x2Au;
    uint8_t out = 0u;
    size_t out_len = 0u;
    const char *path = "footprint_min_baseline.bin";
    int rc = 1;

    memset(&storage, 0, sizeof(storage));
    memset(&cfg, 0, sizeof(cfg));
    memset(&db, 0, sizeof(db));
    lox_port_posix_remove(path);

    if (lox_port_posix_init(&storage, path, 131072u) != LOX_OK) {
        goto cleanup;
    }
    cfg.storage = &storage;
    cfg.ram_kb = 0u;
    if (lox_init(&db, &cfg) != LOX_OK) {
        goto cleanup;
    }
    {
        lox_db_stats_t stats;
        if (lox_get_db_stats(&db, &stats) != LOX_OK || stats.wal_bytes_total == 0u) {
            goto cleanup;
        }
    }
    if (lox_kv_set(&db, "k", &in, 1u, 0u) != LOX_OK) {
        goto cleanup;
    }
    if (lox_kv_get(&db, "k", &out, sizeof(out), &out_len) != LOX_OK || out_len != 1u || out != in) {
        goto cleanup;
    }

    in = 0x5Au;
    if (lox_kv_set(&db, "p", &in, 1u, 0u) != LOX_OK) {
        goto cleanup;
    }
    lox_port_posix_simulate_power_loss(&storage);
    free(lox_core(&db)->heap);
    memset(&db, 0, sizeof(db));
    lox_port_posix_deinit(&storage);
    memset(&storage, 0, sizeof(storage));
    if (lox_port_posix_init(&storage, path, 131072u) != LOX_OK) {
        goto cleanup;
    }
    cfg.storage = &storage;
    if (lox_init(&db, &cfg) != LOX_OK) {
        goto cleanup;
    }
    out = 0u;
    out_len = 0u;
    if (lox_kv_get(&db, "p", &out, sizeof(out), &out_len) != LOX_OK || out_len != 1u || out != in) {
        goto cleanup;
    }
    if (lox_deinit(&db) != LOX_OK) {
        memset(&db, 0, sizeof(db));
        goto cleanup;
    }
    memset(&db, 0, sizeof(db));

    if (lox_init(&db, &cfg) != LOX_OK) {
        goto cleanup;
    }
    in = 0x2Au;
    out = 0u;
    out_len = 0u;
    if (lox_kv_get(&db, "k", &out, sizeof(out), &out_len) != LOX_OK || out_len != 1u || out != in) {
        goto cleanup;
    }

    rc = 0;

cleanup:
    (void)lox_deinit(&db);
    lox_port_posix_deinit(&storage);
    lox_port_posix_remove(path);
    return rc;
}
