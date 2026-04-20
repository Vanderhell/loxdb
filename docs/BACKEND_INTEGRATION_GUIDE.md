# Backend Integration Guide

This guide is the practical entry point for integrating `microdb` storage backends.

It explains:

- how to connect a raw `microdb_storage_t` directly to `microdb_init`
- how to use optional backend-open orchestration (`descriptor -> decision -> adapter`)
- what backend stubs are (and are not)
- where to start when writing a real platform port

For real port authoring, see:

- `docs/PORT_AUTHORING_GUIDE.md`
- `port/esp32/microdb_port_esp32.c` (live reference implementation)

## 1) Two integration modes

### A) Direct mode (minimal path)

Use this when your storage already satisfies core contract requirements:

- `erase_size > 0`
- `write_size == 1`
- stable `read/write/erase/sync` semantics

Flow:

1. Prepare `microdb_storage_t` from your platform driver.
2. Put it into `microdb_cfg_t.storage`.
3. Call `microdb_init`.

### B) Backend-open mode (policy path)

Use this when you support multiple storage classes, or when media may need adapter wrapping.

Flow:

1. Reset session (`microdb_backend_open_session_reset`).
2. Call `microdb_backend_open_prepare(...)`.
3. Use returned `out_storage` in `microdb_cfg_t.storage`.
4. Call `microdb_init`.
5. On shutdown, call `microdb_backend_open_release(...)`.

## 2) Reference backend-open integration

```c
#include "microdb.h"
#include "microdb_backend_open.h"

microdb_t db;
microdb_cfg_t cfg = {0};
microdb_backend_open_session_t session;
microdb_storage_t raw_storage;       /* filled by your platform */
microdb_storage_t *opened = NULL;

microdb_backend_open_session_reset(&session);

if (microdb_backend_open_prepare("my_backend",
                                 &raw_storage,
                                 1u,   /* has_aligned_adapter */
                                 1u,   /* has_managed_adapter */
                                 &session,
                                 &opened) != MICRODB_OK) {
    /* handle open-classification/adapter failure */
}

cfg.storage = opened;
cfg.ram_kb = 32u;

if (microdb_init(&db, &cfg) != MICRODB_OK) {
    microdb_backend_open_release(&session);
    /* handle init failure */
}

/* ... use db ... */

microdb_deinit(&db);
microdb_backend_open_release(&session);
```

## 3) Important: backend stubs are not media drivers

Modules like `*_stub` are capability descriptors used by decision/orchestration and test matrices.

They do **not** provide:

- real hardware I/O implementation
- NAND ECC
- bad block management
- wear leveling

For production media integration, you still must provide real driver glue in `microdb_storage_t` hooks.

## 4) Which optional adapter path to use

- Aligned-write media: use aligned adapter path (RMW/byte-write shim).
- Managed media with durable sync semantics: managed adapter path.
- Filesystem/block-like path with non-durable flush semantics: filesystem adapter policy path.

Detailed contract:

- `docs/FS_BLOCK_ADAPTER_CONTRACT.md`
- `docs/PROGRAMMER_MANUAL.md` (backend APIs and open wrapper sections)
- `examples/aligned_block_port/main.c` (reference aligned/block glue skeleton)

## 5) Common pitfalls

- Calling `microdb_init` directly with unsupported geometry (`write_size != 1`) without adapter path.
- Assuming stubs are plug-and-play drivers.
- Forgetting `microdb_backend_open_release` after backend-open session use.
- Mixing adapter policy assumptions without explicit expectations/checks.

## 6) Bring-up checklist

1. Validate raw storage hooks (`read/write/erase/sync`) with small smoke test.
2. Decide direct mode vs backend-open mode.
3. Verify open/reopen/power-loss behavior with corresponding backend tests.
4. Run CI lane including `test_backend_open`, recovery tests, and stress/matrix slices.
