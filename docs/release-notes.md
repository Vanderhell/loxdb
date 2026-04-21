# Release Notes

## Title

`microdb v1.3.0`

## Release text (GitHub Release body)

This release focuses on production hardening for ESP32-class deployments and closes the remaining correctness gaps found during real-data validation.

Highlights:

- Fixed KV JSON import/export TTL sentinel handling for non-expiring keys (`ttl=0` round-trip).
- Fixed KV admission preflight complexity (`microdb_admit_kv_set`) from O(n) scan to O(1) counter path.
- Added/expanded real-data integration coverage (host + ESP32 `run_real` smoke flow).
- Added explicit WAL sync-mode guidance with measured ESP32 values.
- Improved benchmark interpretation docs (POSIX vs ESP32 behavior).
- Cleaned repository history from tracked ESP32 build artifacts.

Real hardware validation (ESP32-S3, COM17):

- `[REAL_DATA] PASS` end-to-end flow.
- Lifecycle cost measured in flow:
  - `flush` ~7033us
  - `deinit` ~7198us
  - `reinit` ~9716us
- `admit_kv_set` reduced from ~498us to ~190us after O(1) fix.

Known non-blocking follow-ups:

- TS adaptive arena for mixed stream types.
- POSIX benchmark fidelity improvements (`writev`/noise control).
- RTOS port documentation polish (FreeRTOS/Zephyr).

## Contract links

- `README.md`
- `CHANGELOG.md`
- `RELEASE_LOG.md`
- `docs/GETTING_STARTED_5_MIN.md`
- `docs/FAIL_CODE_CONTRACT.md`
- `docs/PROFILE_GUARANTEES.md`
- `docs/OFFLINE_VERIFIER.md`

## Repository topics reference

See `docs/repository-topics.md`.
