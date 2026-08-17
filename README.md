# loxdb

> Predictable-memory database for microcontrollers. KV + time-series + relational, bounded core allocation, WAL recovery.

[![CI](https://github.com/Vanderhell/loxdb/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/loxdb/actions/workflows/ci.yml)
[![Language: C99](https://img.shields.io/badge/language-C99-blue)](https://en.wikipedia.org/wiki/C99)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Platform: MCU | Linux | Windows | macOS](https://img.shields.io/badge/platform-MCU%20%7C%20Linux%20%7C%20Windows%20%7C%20macOS-informational)](https://github.com/Vanderhell/loxdb)
[![Tests](https://img.shields.io/badge/tests-595%20microtests-brightgreen)](docs/TEST_SUITE_SIZE.md)
[![Release](https://img.shields.io/github/v/release/Vanderhell/loxdb)](https://github.com/Vanderhell/loxdb/releases)

## What is loxdb?

loxdb is a compact embedded database written in C99 for firmware and small edge runtimes.
It provides one unified API over three engines (KV, time-series, relational) and is designed around predictable memory behavior.
On successful initialization, the core makes one main heap allocation and has
no allocator churn during normal database operations; `lox_deinit()` releases
that heap. Ports and user callbacks may allocate independently and are outside
this core guarantee.
Persistence is optional via a small storage HAL (read/write/erase/sync), with WAL + recovery when enabled.

Test suite size: **595 microtest cases across 61 C test files, plus one C++ wrapper test.**

## Release artifacts

- GitHub "Source code (zip/tar.gz)" assets are automatic tag snapshots (source tree).
- `loxdb-source-vX.Y.Z.zip` is the explicit source distribution asset (source + tests + tooling).
- `loxdb-sdk-vX.Y.Z-<platform>.(zip|tar.gz)` assets are install-prefix SDK bundles (headers + libraries + CMake package files) and are not expected to include `src/*.c`.

## Why loxdb? (When to use / when not to)

| Use loxdb when you need... | Avoid loxdb when you need... |
|---|---|
| bounded RAM and predictable allocation behavior | unbounded queries / SQL flexibility |
| durability with WAL recovery on flash-like media | a full SQL database with complex query planning |
| KV + telemetry streams + small indexed tables in one library | multi-process concurrency / server database features |
| a small storage HAL integration | transparent large-object storage and advanced indexing |

## Quick start (RAM-backed)

```c
#include "lox.h"
#include "lox_port_ram.h"

int main(void) {
    static lox_t db; /* Keep the potentially multi-KiB handle off small stacks. */
    lox_storage_t storage;
    lox_cfg_t cfg = {0};

    if (lox_port_ram_init(&storage, 64u * 1024u) != LOX_OK) return 1;

    cfg.storage = &storage;
    cfg.ram_kb = 32u;

    if (lox_init(&db, &cfg) != LOX_OK) {
        lox_port_ram_deinit(&storage);
        return 1;
    }

    uint8_t v = 7u, out = 0u;
    size_t out_len = 0u;

    if (lox_kv_put(&db, "k", &v, sizeof(v)) != LOX_OK) {
        lox_deinit(&db);
        lox_port_ram_deinit(&storage);
        return 1;
    }

    if (lox_kv_get(&db, "k", &out, sizeof(out), &out_len) != LOX_OK) {
        lox_deinit(&db);
        lox_port_ram_deinit(&storage);
        return 1;
    }

    lox_deinit(&db);
    lox_port_ram_deinit(&storage);
    return 0;
}
```

## Build & test (desktop)

```bash
cmake --preset ci-debug-linux
cmake --build --preset ci-debug-linux
ctest --preset ci-debug-linux
```

## Three engines in 30 seconds

- **KV (key-value):** config/state, binary-safe values, optional TTL, bounded by compile-time limits.
- **TS (time-series):** typed telemetry streams (`F32/I32/U32/RAW`) with timestamp range queries and retention policies.
- **REL (relational):** small fixed-schema tables with one indexed column, designed for predictable memory use.

## Verification scope

Hardware-specific claims are separated from source claims in [`docs/EVIDENCE_MATRIX.md`](docs/EVIDENCE_MATRIX.md).

The published ESP32-S3 measurements use an in-RAM flash-like storage backend.
They are CPU and logical-backend measurements, not physical NOR, brownout, or
endurance validation. See [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md).

## Project status & roadmap

- Latest release: `v1.5.2`. The `1.5.3` line is in preparation for Arduino
  Library Manager and PlatformIO Registry publication (see `CHANGELOG.md`).
- Verification status is tracked explicitly in [`docs/EVIDENCE_MATRIX.md`](docs/EVIDENCE_MATRIX.md).

## Documentation

- API reference: `docs/API_REFERENCE.md`
- Cookbook: `docs/COOKBOOK.md`
- Getting started: `docs/GETTING_STARTED_5_MIN.md`
- Programmer manual: `docs/PROGRAMMER_MANUAL.md`
- Backend integration: `docs/BACKEND_INTEGRATION_GUIDE.md`
- Port authoring (ESP32 reference): `docs/PORT_AUTHORING_GUIDE.md`
- Schema migration: `docs/SCHEMA_MIGRATION_GUIDE.md`
- Evidence matrix: `docs/EVIDENCE_MATRIX.md`
- Docs index: `docs/README.md`

## Contributing & support

- Contributing guide: `.github/CONTRIBUTING.md`
- Support policy: `.github/SUPPORT.md`
- Security policy: `.github/SECURITY.md`

## License

MIT (see `LICENSE`).
