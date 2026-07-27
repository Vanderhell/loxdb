# Release Notes Draft (Next Release)

## Title

`loxdb v1.5.1`

## Release text (GitHub Release body)

This focused patch release checks persisted TS timestamp width during recovery
and removes unused WAL storage from WAL-disabled builds.

Highlights:

- TS snapshot loading and WAL replay preserve the fixed 64-bit persisted value
  when it fits `lox_timestamp_t`, and return `LOX_ERR_OVERFLOW` without
  truncation when it does not.
- `LOX_ENABLE_WAL=0` layouts report and reserve zero WAL bytes while retaining
  dual-bank snapshot persistence.
- WAL-enabled layouts and `LOX_PROFILE_FOOTPRINT_MIN` behavior are unchanged.

No-WAL media created by earlier releases uses different physical offsets
because those releases reserved an unused minimum WAL region. The v1.5.1
no-WAL layout starts its first superblock at offset zero.

## Contract links

- `README.md`
- `CHANGELOG.md`
- `docs/API_REFERENCE.md`
- `docs/PROGRAMMER_MANUAL.md`
- `docs/PROFILES.md`
- `docs/EVIDENCE_MATRIX.md`
