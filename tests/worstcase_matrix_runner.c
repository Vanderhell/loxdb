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
    if (wipe_file) microdb_port_posix_remove(cfg->path);
    memset(&g_db, 0, sizeof(g_db));
    memset(&g_storage, 0, sizeof(g_storage));
    if (microdb_port_posix_init(&g_storage, cfg->path, 262144u) != MICRODB_OK) return 0;
    memset(&db_cfg, 0, sizeof(db_cfg));
    db_cfg.storage = &g_storage;
    db_cfg.ram_kb = cfg->ram_kb;
    return microdb_init(&g_db, &db_cfg) == MICRODB_OK;
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
    if (microdb_deinit(&g_db) != MICRODB_OK) return 0;
    microdb_port_posix_deinit(&g_storage);
    if (!open_db(cfg, 0)) return 0;
    *dt_us = now_us() - t0;
    return 1;
}

static int ensure_ts_stream(void) {
    microdb_err_t rc = microdb_ts_register(&g_db, "s", MICRODB_TS_U32, 0u);
    return rc == MICRODB_OK || rc == MICRODB_ERR_EXISTS;
}

static int ensure_rel_table(microdb_table_t **out) {
    microdb_schema_t s;
    microdb_err_t rc = microdb_table_get(&g_db, "m", out);
    if (rc == MICRODB_OK) return 1;
    if (rc != MICRODB_ERR_NOT_FOUND) return 0;
    if (microdb_schema_init(&s, "m", 96u) != MICRODB_OK) return 0;
    if (microdb_schema_add(&s, "id", MICRODB_COL_U32, sizeof(uint32_t), true) != MICRODB_OK) return 0;
    if (microdb_schema_add(&s, "v", MICRODB_COL_U8, sizeof(uint8_t), false) != MICRODB_OK) return 0;
    if (microdb_schema_seal(&s) != MICRODB_OK) return 0;
    rc = microdb_table_create(&g_db, &s);
    if (rc != MICRODB_OK && rc != MICRODB_ERR_EXISTS) return 0;
    return microdb_table_get(&g_db, "m", out) == MICRODB_OK;
}

static int warm_to_fill(uint32_t fill_target_pct, uint32_t warmup_ops, uint32_t *out_fill) {
    uint32_t i;
    microdb_stats_t st;
    uint8_t v[24];
    char key[24];

    memset(v, 0, sizeof(v));
    for (i = 0u; i < warmup_ops; ++i) {
        uint32_t seq = i + 1u;
        memcpy(v, &seq, sizeof(seq));
        (void)snprintf(key, sizeof(key), "w%04u", (unsigned)(i % 320u));
        if (microdb_kv_put(&g_db, key, v, sizeof(v)) != MICRODB_OK) return 0;
        if ((i % 64u) == 0u) {
            if (microdb_inspect(&g_db, &st) != MICRODB_OK) return 0;
            if (st.wal_fill_pct >= fill_target_pct) {
                *out_fill = st.wal_fill_pct;
                return 1;
            }
        }
    }
    if (microdb_inspect(&g_db, &st) != MICRODB_OK) return 0;
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

    if (!ensure_ts_stream()) return 0;
    if (!ensure_rel_table(&t)) return 0;

    for (i = 0u; i < age_ops; ++i) {
        uint32_t op = rnd_next() % 4u;
        if (op == 0u) {
            kv = i ^ 0x55AA55AAu;
            (void)snprintf(key, sizeof(key), "a%03u", (unsigned)(i % 96u));
            if (microdb_kv_put(&g_db, key, &kv, sizeof(kv)) != MICRODB_OK) return 0;
        } else if (op == 1u) {
            kv = i;
            if (microdb_ts_insert(&g_db, "s", i + 1u, &kv) != MICRODB_OK) return 0;
        } else if (op == 2u) {
            id = i % 220u;
            v8 = (uint8_t)(id & 0xFFu);
            memset(row, 0, sizeof(row));
            if (microdb_row_set(t, row, "id", &id) != MICRODB_OK) return 0;
            if (microdb_row_set(t, row, "v", &v8) != MICRODB_OK) return 0;
            (void)microdb_rel_insert(&g_db, t, row);
        } else {
            id = i % 220u;
            (void)microdb_rel_delete(&g_db, t, &id, NULL);
        }
        if ((i % 128u) == 0u) {
            if (microdb_flush(&g_db) != MICRODB_OK) return 0;
        }
    }

    for (i = 0u; i < cycle_count; ++i) {
        if (microdb_compact(&g_db) != MICRODB_OK) return 0;
        if (microdb_flush(&g_db) != MICRODB_OK) return 0;
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
        if (microdb_kv_put(&g_db, key, vbuf, sizeof(vbuf)) != MICRODB_OK) {
            out->fail_count++;
            return 0;
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_kv_put_us) out->max_kv_put_us = dt;
    }

    for (i = 0u; i < cfg->measure_ops; ++i) {
        uint32_t tsv = i;
        t0 = now_us();
        if (microdb_ts_insert(&g_db, "s", 100000u + i, &tsv) != MICRODB_OK) {
            out->fail_count++;
            return 0;
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
        if (microdb_row_set(t, row, "id", &id) != MICRODB_OK) {
            out->fail_count++;
            return 0;
        }
        if (microdb_row_set(t, row, "v", &v) != MICRODB_OK) {
            out->fail_count++;
            return 0;
        }
        t0 = now_us();
        if (microdb_rel_insert(&g_db, t, row) != MICRODB_OK) {
            if (microdb_rel_clear(&g_db, t) != MICRODB_OK) return 0;
            if (microdb_rel_insert(&g_db, t, row) != MICRODB_OK) return 0;
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_rel_insert_us) out->max_rel_insert_us = dt;
    }

    for (i = 0u; i < cfg->measure_ops; ++i) {
        uint32_t id = id_seed + i;
        t0 = now_us();
        if (microdb_rel_delete(&g_db, t, &id, NULL) != MICRODB_OK) {
            out->fail_count++;
            return 0;
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_rel_delete_us) out->max_rel_delete_us = dt;
    }

    for (i = 0u; i < cfg->measure_ops; ++i) {
        uint32_t tv = i ^ 0x5AA5u;
        if (microdb_txn_begin(&g_db) != MICRODB_OK) {
            out->fail_count++;
            return 0;
        }
        (void)snprintf(key, sizeof(key), "t%03u", (unsigned)(i % 64u));
        if (microdb_kv_put(&g_db, key, &tv, sizeof(tv)) != MICRODB_OK) {
            out->fail_count++;
            return 0;
        }
        t0 = now_us();
        if (microdb_txn_commit(&g_db) != MICRODB_OK) {
            out->fail_count++;
            return 0;
        }
        dt = now_us() - t0;
        if (dt > 1000u) out->spikes_gt_1ms++;
        if (dt > 5000u) out->spikes_gt_5ms++;
        if (dt > out->max_txn_commit_us) out->max_txn_commit_us = dt;
    }

    t0 = now_us();
    if (microdb_compact(&g_db) != MICRODB_OK) return 0;
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

    if (microdb_deinit(&g_db) != MICRODB_OK) return 0;
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
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 2;
        }
    }

    printf("profile,phase,fill_target,fill_reached,max_kv_put_us,max_ts_insert_us,max_rel_insert_us,max_rel_delete_us,max_txn_commit_us,max_compact_us,max_reopen_us,spikes_gt_1ms,spikes_gt_5ms,fail_count,slo_pass\n");

    for (i = 0u; i < sizeof(fills) / sizeof(fills[0]); ++i) {
        if (!run_measure(&cfg, fills[i], "fresh", &row)) {
            fprintf(stderr, "matrix failed for fresh fill=%u\n", fills[i]);
            return 1;
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
            fprintf(stderr, "matrix failed for aged fill=%u\n", fills[i]);
            return 1;
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
