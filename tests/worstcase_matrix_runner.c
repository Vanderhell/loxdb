// SPDX-License-Identifier: MIT
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "microdb.h"
#include "../port/posix/microdb_port_posix.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
static uint64_t now_us(void) {
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (uint64_t)((t.QuadPart * 1000000ull) / freq.QuadPart);
}
#else
#include <time.h>
static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)(ts.tv_nsec / 1000ull);
}
#endif

typedef struct {
    const char *profile;
    uint32_t ram_kb;
    uint32_t warmup_ops;
    uint32_t measure_ops;
    uint32_t age_ops;
    uint32_t cycle_count;
    uint32_t slo_max_op_us;
    uint32_t slo_max_compact_us;
    uint32_t slo_max_reopen_us;
    uint32_t slo_spike5_limit;
    char path[128];
} cfg_t;

typedef struct {
    const char *phase;
    const char *profile;
    uint32_t fill_target_pct;
    uint32_t fill_reached_pct;
    uint64_t max_kv_put_us;
    uint64_t max_ts_insert_us;
    uint64_t max_rel_insert_us;
    uint64_t max_rel_delete_us;
    uint64_t max_txn_commit_us;
    uint64_t max_compact_us;
    uint64_t max_reopen_us;
    uint64_t spikes_gt_1ms;
    uint64_t spikes_gt_5ms;
    uint64_t fail_count;
    uint32_t slo_pass;
} row_t;

static microdb_t g_db;
static microdb_storage_t g_storage;
static uint32_t g_rng = 0x12345678u;

static int fail_status(const char *op, const char *status_name, int status_code, const char *detail) {
    fprintf(stderr, "%s failed: %s (%d)", op, status_name, status_code);
    if (detail != NULL && detail[0] != '\0') {
        fprintf(stderr, " - %s", detail);
    }
    fprintf(stderr, "\n");
    return status_code;
}

static int fail_microdb(const char *op, microdb_err_t rc, int ret) {
    fprintf(stderr, "%s failed: %s (%d)\n", op, microdb_err_to_string(rc), (int)rc);
    return ret;
}

static uint32_t rnd_next(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng;
}

static int parse_u32_arg(const char *arg, uint32_t *out) {
    char *end = NULL;
    unsigned long v = strtoul(arg, &end, 10);
    if (arg == NULL || *arg == '\0' || end == NULL || *end != '\0') return 0;
    *out = (uint32_t)v;
    return 1;
}

static int open_db(const cfg_t *cfg, int wipe_file) {
    microdb_cfg_t db_cfg;
    microdb_err_t rc;
    if (wipe_file) microdb_port_posix_remove(cfg->path);
    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    rc = microdb_port_posix_init(&g_storage, cfg->path, 262144u);
    if (rc != MICRODB_OK) return fail_microdb("microdb_port_posix_init", rc, 0);
    memset(&db_cfg, 0, sizeof(db_cfg));
    db_cfg.storage = &g_storage;
    db_cfg.ram_kb = cfg->ram_kb;
    rc = microdb_init(&g_db, &db_cfg);
    if (rc != MICRODB_OK) return fail_microdb("microdb_init", rc, 0);
    return 1;
}

static void set_profile_defaults(cfg_t *cfg, const char *profile) {
    cfg->profile = profile;
    if (strcmp(profile, "deterministic") == 0) {
        cfg->ram_kb = 48u;
        cfg->warmup_ops = 9000u;
        cfg->measure_ops = 192u;
        cfg->age_ops = 10000u;
        cfg->cycle_count = 20u;
        /* Desktop host jitter can produce occasional long-tail stalls. */
        cfg->slo_max_op_us = 120000u;
        cfg->slo_max_compact_us = 45000u;
        cfg->slo_max_reopen_us = 140000u;
        cfg->slo_spike5_limit = 1000u;
    } else if (strcmp(profile, "stress") == 0) {
        cfg->ram_kb = 96u;
        cfg->warmup_ops = 24000u;
        cfg->measure_ops = 320u;
        cfg->age_ops = 18000u;
        cfg->cycle_count = 36u;
        cfg->slo_max_op_us = 70000u;
        cfg->slo_max_compact_us = 70000u;
        cfg->slo_max_reopen_us = 130000u;
        cfg->slo_spike5_limit = 3000u;
    } else {
        cfg->profile = "balanced";
        cfg->ram_kb = 64u;
        cfg->warmup_ops = 16000u;
        cfg->measure_ops = 256u;
        cfg->age_ops = 12000u;
        cfg->cycle_count = 24u;
        cfg->slo_max_op_us = 50000u;
        cfg->slo_max_compact_us = 50000u;
        cfg->slo_max_reopen_us = 100000u;
        cfg->slo_spike5_limit = 1800u;
    }
}

static int reopen_db(const cfg_t *cfg, uint64_t *dt_us) {
    uint64_t t0 = now_us();
    microdb_err_t rc = microdb_deinit(&g_db);
    if (rc != MICRODB_OK) return fail_microdb("microdb_deinit", rc, 0);
    microdb_port_posix_deinit(&g_storage);
    if (!open_db(cfg, 0)) return 0;
    *dt_us = now_us() - t0;
    return 1;
}

static int ensure_ts_stream(void) {
    microdb_err_t rc = microdb_ts_register(&g_db, "s", MICRODB_TS_U32, 0u);
    if (rc == MICRODB_OK || rc == MICRODB_ERR_EXISTS) return 1;
    return fail_microdb("microdb_ts_register(s)", rc, 0);
}

static int ensure_rel_table(microdb_table_t **out) {
    microdb_schema_t s;
    microdb_err_t rc = microdb_table_get(&g_db, "m", out);
    if (rc == MICRODB_OK) return 1;
    if (rc != MICRODB_ERR_NOT_FOUND) return fail_microdb("microdb_table_get(m)", rc, 0);
    rc = microdb_schema_init(&s, "m", 96u);
    if (rc != MICRODB_OK) return fail_microdb("microdb_schema_init(m)", rc, 0);
    rc = microdb_schema_add(&s, "id", MICRODB_COL_U32, sizeof(uint32_t), true);
    if (rc != MICRODB_OK) return fail_microdb("microdb_schema_add(m.id)", rc, 0);
    rc = microdb_schema_add(&s, "v", MICRODB_COL_U8, sizeof(uint8_t), false);
    if (rc != MICRODB_OK) return fail_microdb("microdb_schema_add(m.v)", rc, 0);
    rc = microdb_schema_seal(&s);
    if (rc != MICRODB_OK) return fail_microdb("microdb_schema_seal(m)", rc, 0);
    rc = microdb_table_create(&g_db, &s);
    if (rc != MICRODB_OK && rc != MICRODB_ERR_EXISTS) return fail_microdb("microdb_table_create(m)", rc, 0);
    rc = microdb_table_get(&g_db, "m", out);
    if (rc != MICRODB_OK) return fail_microdb("microdb_table_get(m,created)", rc, 0);
    return 1;
}

static int warm_to_fill(uint32_t fill_target_pct, uint32_t warmup_ops, uint32_t *out_fill) {
    uint32_t i;
    microdb_stats_t st;
    uint8_t v[24];
    char key[24];
    microdb_err_t rc;

    memset(v, 0, sizeof(v));
    for (i = 0u; i < warmup_ops; ++i) {
        uint32_t seq = i + 1u;
        memcpy(v, &seq, sizeof(seq));
        (void)snprintf(key, sizeof(key), "w%04u", (unsigned)(i % 320u));
        rc = microdb_kv_put(&g_db, key, v, sizeof(v));
        if (rc != MICRODB_OK) return fail_microdb("microdb_kv_put(warm_to_fill)", rc, 0);
        if ((i % 64u) == 0u) {
            rc = microdb_inspect(&g_db, &st);
            if (rc != MICRODB_OK) return fail_microdb("microdb_inspect(warm_to_fill)", rc, 0);
            if (st.wal_fill_pct >= fill_target_pct) {
                *out_fill = st.wal_fill_pct;
                return 1;
            }
        }
    }
    rc = microdb_inspect(&g_db, &st);
    if (rc != MICRODB_OK) return fail_microdb("microdb_inspect(warm_to_fill,final)", rc, 0);
    *out_fill = st.wal_fill_pct;
    return 1;
}

static int age_workload(uint32_t age_ops, uint32_t cycle_count) {
    uint32_t i;
    microdb_table_t *t = NULL;
    uint8_t row[64];
    uint32_t id;
    uint8_t v8;
    uint32_t kv;
    char key[16];
    microdb_err_t rc;

    if (!ensure_ts_stream()) return 0;
    if (!ensure_rel_table(&t)) return 0;

    for (i = 0u; i < age_ops; ++i) {
        uint32_t op = rnd_next() % 4u;
        if (op == 0u) {
            kv = i ^ 0x55AA55AAu;
            (void)snprintf(key, sizeof(key), "a%03u", (unsigned)(i % 96u));
            rc = microdb_kv_put(&g_db, key, &kv, sizeof(kv));
            if (rc != MICRODB_OK) return fail_microdb("microdb_kv_put(age)", rc, 0);
        } else if (op == 1u) {
            kv = i;
            rc = microdb_ts_insert(&g_db, "s", i + 1u, &kv);
            if (rc != MICRODB_OK) return fail_microdb("microdb_ts_insert(age)", rc, 0);
        } else if (op == 2u) {
            id = i % 220u;
            v8 = (uint8_t)(id & 0xFFu);
            memset(row, 0, sizeof(row));
            rc = microdb_row_set(t, row, "id", &id);
            if (rc != MICRODB_OK) return fail_microdb("microdb_row_set(age,id)", rc, 0);
            rc = microdb_row_set(t, row, "v", &v8);
            if (rc != MICRODB_OK) return fail_microdb("microdb_row_set(age,v)", rc, 0);
            (void)microdb_rel_insert(&g_db, t, row);
        } else {
            id = i % 220u;
            (void)microdb_rel_delete(&g_db, t, &id, NULL);
        }
        if ((i % 128u) == 0u) {
            rc = microdb_flush(&g_db);
            if (rc != MICRODB_OK) return fail_microdb("microdb_flush(age)", rc, 0);
        }
    }

    for (i = 0u; i < cycle_count; ++i) {
        rc = microdb_compact(&g_db);
        if (rc != MICRODB_OK) return fail_microdb("microdb_compact(age)", rc, 0);
        rc = microdb_flush(&g_db);
        if (rc != MICRODB_OK) return fail_microdb("microdb_flush(age,cycle)", rc, 0);
    }
    return 1;
}

static int run_measure(const cfg_t *cfg, uint32_t fill_target_pct, const char *phase, row_t *out) {
    uint32_t i;
    uint32_t fill_reached = 0u;
    uint8_t vbuf[16];
    char key[24];
    microdb_table_t *t = NULL;
    uint8_t row[64];
    uint64_t t0;
    uint64_t dt;
    uint32_t id_seed = 1000u;
    microdb_err_t rc;

    memset(out, 0, sizeof(*out));
    out->phase = phase;
    out->profile = cfg->profile;
    out->fill_target_pct = fill_target_pct;

    if (!open_db(cfg, 1)) return 0;
    if (!ensure_ts_stream()) return 0;
    if (!ensure_rel_table(&t)) return 0;

    if (strcmp(phase, "aged") == 0) {
        if (!age_workload(cfg->age_ops, cfg->cycle_count)) return 0;
    }

    if (!warm_to_fill(fill_target_pct, cfg->warmup_ops, &fill_reached)) return 0;
    out->fill_reached_pct = fill_reached;

    memset(vbuf, 0xA5, sizeof(vbuf));

    for (i = 0u; i < cfg->measure_ops; ++i) {
        t0 = now_us();
        (void)snprintf(key, sizeof(key), "k%03u", (unsigned)(i % 96u));
        rc = microdb_kv_put(&g_db, key, vbuf, sizeof(vbuf));
        if (rc != MICRODB_OK) {
            out->fail_count++;
            return fail_microdb("microdb_kv_put(measure)", rc, 0);
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_kv_put_us) out->max_kv_put_us = dt;
    }

    for (i = 0u; i < cfg->measure_ops; ++i) {
        uint32_t tsv = i;
        t0 = now_us();
        rc = microdb_ts_insert(&g_db, "s", 100000u + i, &tsv);
        if (rc != MICRODB_OK) {
            out->fail_count++;
            return fail_microdb("microdb_ts_insert(measure)", rc, 0);
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_ts_insert_us) out->max_ts_insert_us = dt;
    }

    for (i = 0u; i < cfg->measure_ops; ++i) {
        uint32_t id = id_seed + i;
        uint8_t v = (uint8_t)(id & 0xFFu);
        memset(row, 0, sizeof(row));
        rc = microdb_row_set(t, row, "id", &id);
        if (rc != MICRODB_OK) {
            out->fail_count++;
            return fail_microdb("microdb_row_set(measure,id)", rc, 0);
        }
        rc = microdb_row_set(t, row, "v", &v);
        if (rc != MICRODB_OK) {
            out->fail_count++;
            return fail_microdb("microdb_row_set(measure,v)", rc, 0);
        }
        t0 = now_us();
        rc = microdb_rel_insert(&g_db, t, row);
        if (rc != MICRODB_OK) {
            rc = microdb_rel_clear(&g_db, t);
            if (rc != MICRODB_OK) return fail_microdb("microdb_rel_clear(measure)", rc, 0);
            rc = microdb_rel_insert(&g_db, t, row);
            if (rc != MICRODB_OK) return fail_microdb("microdb_rel_insert(measure,retry)", rc, 0);
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_rel_insert_us) out->max_rel_insert_us = dt;
    }

    for (i = 0u; i < cfg->measure_ops; ++i) {
        uint32_t id = id_seed + i;
        t0 = now_us();
        rc = microdb_rel_delete(&g_db, t, &id, NULL);
        if (rc != MICRODB_OK) {
            out->fail_count++;
            return fail_microdb("microdb_rel_delete(measure)", rc, 0);
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_rel_delete_us) out->max_rel_delete_us = dt;
    }

    for (i = 0u; i < cfg->measure_ops; ++i) {
        uint32_t tv = i ^ 0x5AA5u;
        rc = microdb_txn_begin(&g_db);
        if (rc != MICRODB_OK) {
            out->fail_count++;
            return fail_microdb("microdb_txn_begin(measure)", rc, 0);
        }
        (void)snprintf(key, sizeof(key), "t%03u", (unsigned)(i % 64u));
        rc = microdb_kv_put(&g_db, key, &tv, sizeof(tv));
        if (rc != MICRODB_OK) {
            out->fail_count++;
            return fail_microdb("microdb_kv_put(measure,txn)", rc, 0);
        }
        t0 = now_us();
        rc = microdb_txn_commit(&g_db);
        if (rc != MICRODB_OK) {
            out->fail_count++;
            return fail_microdb("microdb_txn_commit(measure)", rc, 0);
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_txn_commit_us) out->max_txn_commit_us = dt;
    }

    t0 = now_us();
    rc = microdb_compact(&g_db);
    if (rc != MICRODB_OK) return fail_microdb("microdb_compact(measure)", rc, 0);
    out->max_compact_us = now_us() - t0;

    if (!reopen_db(cfg, &out->max_reopen_us)) return 0;
    if (out->max_reopen_us > 1000u) out->spikes_gt_1ms++;
    if (out->max_reopen_us > 5000u) out->spikes_gt_5ms++;

    out->slo_pass =
        (out->max_kv_put_us <= cfg->slo_max_op_us) &&
        (out->max_ts_insert_us <= cfg->slo_max_op_us) &&
        (out->max_rel_insert_us <= cfg->slo_max_op_us) &&
        (out->max_rel_delete_us <= cfg->slo_max_op_us) &&
        (out->max_txn_commit_us <= cfg->slo_max_op_us) &&
        (out->max_compact_us <= cfg->slo_max_compact_us) &&
        (out->max_reopen_us <= cfg->slo_max_reopen_us) &&
        (out->spikes_gt_5ms <= cfg->slo_spike5_limit) &&
        (out->fail_count == 0u);

    rc = microdb_deinit(&g_db);
    if (rc != MICRODB_OK) return fail_microdb("microdb_deinit(measure)", rc, 0);
    microdb_port_posix_deinit(&g_storage);
    microdb_port_posix_remove(cfg->path);
    return 1;
}

int main(int argc, char **argv) {
    cfg_t cfg;
    uint32_t fills[] = {10u, 50u, 75u, 90u, 95u};
    size_t i;
    row_t row;

    memset(&cfg, 0, sizeof(cfg));
    set_profile_defaults(&cfg, "balanced");
    strcpy(cfg.path, "worstcase_matrix.bin");

    for (i = 1u; i < (size_t)argc; ++i) {
        if (strcmp(argv[i], "--warmup-ops") == 0 && i + 1u < (size_t)argc) {
            if (!parse_u32_arg(argv[++i], &cfg.warmup_ops)) return 2;
        } else if (strcmp(argv[i], "--measure-ops") == 0 && i + 1u < (size_t)argc) {
            if (!parse_u32_arg(argv[++i], &cfg.measure_ops)) return 2;
        } else if (strcmp(argv[i], "--age-ops") == 0 && i + 1u < (size_t)argc) {
            if (!parse_u32_arg(argv[++i], &cfg.age_ops)) return 2;
        } else if (strcmp(argv[i], "--cycles") == 0 && i + 1u < (size_t)argc) {
            if (!parse_u32_arg(argv[++i], &cfg.cycle_count)) return 2;
        } else if (strcmp(argv[i], "--path") == 0 && i + 1u < (size_t)argc) {
            strncpy(cfg.path, argv[++i], sizeof(cfg.path) - 1u);
            cfg.path[sizeof(cfg.path) - 1u] = '\0';
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1u < (size_t)argc) {
            set_profile_defaults(&cfg, argv[++i]);
        } else {
            char detail[256];
            (void)snprintf(detail,
                           sizeof(detail),
                           "unknown arg '%s'; usage: --warmup-ops N --measure-ops N --age-ops N --cycles N --path FILE --profile balanced|deterministic|stress",
                           argv[i]);
            return fail_status("parse_args", "EXIT_USAGE", 2, detail);
        }
    }

    printf("profile,phase,fill_target,fill_reached,max_kv_put_us,max_ts_insert_us,max_rel_insert_us,max_rel_delete_us,max_txn_commit_us,max_compact_us,max_reopen_us,spikes_gt_1ms,spikes_gt_5ms,fail_count,slo_pass\n");

    for (i = 0u; i < sizeof(fills) / sizeof(fills[0]); ++i) {
        if (!run_measure(&cfg, fills[i], "fresh", &row)) {
            char detail[128];
            (void)snprintf(detail, sizeof(detail), "matrix failed for fresh fill=%u", (unsigned)fills[i]);
            return fail_status("run_measure", "EXIT_FAILURE", 1, detail);
        }
        printf("%s,%s,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u\n",
               row.profile,
               row.phase,
               (unsigned)row.fill_target_pct,
               (unsigned)row.fill_reached_pct,
               (unsigned long long)row.max_kv_put_us,
               (unsigned long long)row.max_ts_insert_us,
               (unsigned long long)row.max_rel_insert_us,
               (unsigned long long)row.max_rel_delete_us,
               (unsigned long long)row.max_txn_commit_us,
               (unsigned long long)row.max_compact_us,
               (unsigned long long)row.max_reopen_us,
               (unsigned long long)row.spikes_gt_1ms,
               (unsigned long long)row.spikes_gt_5ms,
               (unsigned long long)row.fail_count,
               (unsigned)row.slo_pass);

        if (!run_measure(&cfg, fills[i], "aged", &row)) {
            char detail[128];
            (void)snprintf(detail, sizeof(detail), "matrix failed for aged fill=%u", (unsigned)fills[i]);
            return fail_status("run_measure", "EXIT_FAILURE", 1, detail);
        }
        printf("%s,%s,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u\n",
               row.profile,
               row.phase,
               (unsigned)row.fill_target_pct,
               (unsigned)row.fill_reached_pct,
               (unsigned long long)row.max_kv_put_us,
               (unsigned long long)row.max_ts_insert_us,
               (unsigned long long)row.max_rel_insert_us,
               (unsigned long long)row.max_rel_delete_us,
               (unsigned long long)row.max_txn_commit_us,
               (unsigned long long)row.max_compact_us,
               (unsigned long long)row.max_reopen_us,
               (unsigned long long)row.spikes_gt_1ms,
               (unsigned long long)row.spikes_gt_5ms,
               (unsigned long long)row.fail_count,
               (unsigned)row.slo_pass);
    }

    return 0;
}
