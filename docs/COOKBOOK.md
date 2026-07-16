# Cookbook

Short recipes for common integration paths.

## 1. RAM-only database

```c
#include "lox.h"

int main(void) {
    lox_t db;
    lox_cfg_t cfg = {0};

    cfg.ram_kb = 32u;
    if (lox_init(&db, &cfg) != LOX_OK) {
        return 1;
    }

    (void)lox_deinit(&db);
    return 0;
}
```

## 2. Installed C consumer

Use `find_package(loxdb CONFIG REQUIRED)` and link `loxdb::loxdb`.
See `tests/consumer/install_c/` for the detached build recipe used by CTest.

## 3. Installed C++ consumer

Use `find_package(loxdb CONFIG REQUIRED)` and link `loxdb::loxdb`.
See `tests/consumer/install_cpp/` for the detached build recipe used by CTest.

## 4. Package mismatch guard

The installed headers reject mismatched ABI/configuration macros at compile time.
The installed package version file rejects incompatible version requests at
configure time.

## 5. Storage backend bring-up

Start with the backend integration guide:

- `docs/BACKEND_INTEGRATION_GUIDE.md`
- `docs/PORT_AUTHORING_GUIDE.md`

For a practical reference, inspect the port sources in `port/`.

## Verification Status

- Host consumer recipes are verified through the phase-05 detached consumer tests.
- Hardware and vendor-specific bring-up details are NOT VERIFIED here unless a
  concrete result is linked in `docs/EVIDENCE_MATRIX.md`.
