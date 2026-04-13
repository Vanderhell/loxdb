# Footprint-Min Contract

This contract defines the smallest supported durable profile with hard, CI-enforced size gates.

## Profile

- compile-time profile: `MICRODB_PROFILE_FOOTPRINT_MIN=1`
- intended behavior:
  - KV enabled
  - TS disabled
  - REL disabled
  - WAL enabled (power-fail/recovery path remains active)

## Guarantees vs non-guarantees

Guaranteed in `FOOTPRINT_MIN`:

- persistent KV with WAL + snapshot replay
- fail-closed boot on corrupt persisted state
- storage contract checks (`erase_size > 0`, `write_size == 1`)
- close/reopen persistence for KV

Not guaranteed in `FOOTPRINT_MIN`:

- TS engine behavior (disabled)
- REL engine behavior (disabled)
- TS/REL durability semantics (not applicable, disabled)

Important separation:

- `FOOTPRINT_MIN` is the smallest **durable** supported profile.
- `microdb_tiny` (KV-only, WAL-off) is a separate smallest-size variant and has weaker power-fail durability semantics than WAL profiles.
- `microdb_tiny` must not be communicated as equivalent durability to WAL-enabled profiles.

## Canonical baseline

The canonical footprint test is `test_footprint_min_baseline` and does only:

1. `init/open` (with persistent POSIX storage)
2. `1x KV set/get`
3. `close/deinit`
4. `reopen`
5. `1x KV get` (verify persistence/recovery)

No benchmark workload and no extra features.

## Hard section gates (CI fail on breach)

Product size contract (`Release`):

`test_footprint_min_size_gate_release` enforces:

- `.text <= 25000`
- `.rdata <= 1600`
- `.data <= 256`
- `.bss <= 2048`

Sections are parsed from the linker map (`test_footprint_min_baseline.map`).

## Linkage audit (CI fail on breach)

Both size-gate tests also fail if the canonical baseline links forbidden objects:

- `microdb_ts.obj`
- `microdb_rel.obj`
- `microdb_verify.obj`
- `soak_runner.obj`
- `worstcase_matrix_runner.obj`

Required objects for the minimal supported profile:

- `microdb.obj`
- `microdb_kv.obj`
- `microdb_wal.obj`
- `microdb_crc.obj`
