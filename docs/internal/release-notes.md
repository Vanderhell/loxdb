# Release Notes

## Title

`loxdb v1.5.2`

## Release text (GitHub Release body)

This patch release hardens public buffer-boundary contracts, persistence
recovery, backend capability checks, thread-safe metadata access, and the
documented import/export JSON subset.

Highlights:

- Relational setters, indexed lookups, and row getters now require explicit
  lengths and destination capacities, preventing caller-buffer over-read and
  overwrite. This is a source-breaking public API change.
- No-WAL time-series and relational mutations are included in the synchronized
  dual-bank snapshot before successful return.
- Thread-safe relational row-count reads and invalid-handle callback admission
  are synchronized and validated.
- POSIX descriptor zero is handled as a valid descriptor, backend byte-write
  capabilities are validated, and aligned-adapter capacity arithmetic is
  checked.
- Timestamp-width validation, JSON escaping, and strict import/export parsing
  are covered by regression tests.

No-WAL media created by v1.5.1 and earlier uses different physical offsets
because those releases reserved an unused minimum WAL region. The v1.5.2
no-WAL layout starts its first superblock at offset zero, and existing media
must be handled according to the persistence compatibility rules in the API
reference.

## Contract links

- `README.md`
- `CHANGELOG.md`
- `docs/API_REFERENCE.md`
- `docs/PROGRAMMER_MANUAL.md`
- `docs/PROFILES.md`
- `docs/EVIDENCE_MATRIX.md`
