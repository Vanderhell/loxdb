# Changelog

All notable changes to this project are documented in this file.

The format is inspired by Keep a Changelog and follows semantic versioning intent where possible.

## [Unreleased]

### Fixed

- Hardened relational row and indexed-key APIs against caller-buffer over-read
  and overwrite by requiring explicit input lengths and output capacities.
- Corrected no-WAL time-series and relational mutation ordering so successful
  inserts and deletes are included in the synchronized dual-bank snapshot
  before the operation returns.
- Synchronized relational row-count reads in thread-safe builds and prevented
  invalid handles from dispatching configured lock callbacks.
- Corrected the POSIX storage adapter so file descriptor zero is retained and
  closed like every other valid descriptor.
- Enforced unsigned, at-most-64-bit timestamp types at compile time.
- Rejected managed storage backends that cannot provide the byte-write
  capability advertised to the core.
- Escaped time-series stream and relational table names consistently in JSON
  import/export output.
- Hardened import/export field parsing so quoted field names embedded in JSON
  string values cannot be treated as object fields.

### Changed

- Updated the public C and C++ relational APIs to carry value/key lengths and
  destination capacities. This is a source-breaking change; callers must pass
  STR lengths without the terminator and exact sizes for BLOB/scalar values.
- Updated embedded-oriented quick-start examples to keep the database handle
  in static storage rather than consuming an application stack frame.

### Tests

- Added relational buffer-boundary, bounded-string, BLOB/key-length, indexed
  STR/BLOB, and C++ POD type-size regression coverage.
- Added immediate crash/reopen coverage for no-WAL time-series and relational
  table, insert, delete, and clear mutations.
- Added no-WAL snapshot sync-failure coverage for faulted-handle and reopen
  recovery behavior.

## [1.5.1] - 2026-07-27

### Fixed

- Checked fixed-width 64-bit TS timestamps before converting them to the
  configured `lox_timestamp_t` during snapshot loading and WAL replay. Values
  that do not fit now fail reopen with `LOX_ERR_OVERFLOW` instead of truncating.
- Removed the reserved WAL region from `LOX_ENABLE_WAL=0` storage layouts;
  preflight, initialization, statistics, and admission reporting now expose
  zero WAL offset, size, capacity, and usage for those builds.

### Compatibility

- WAL-enabled layouts and the WAL-enabled `LOX_PROFILE_FOOTPRINT_MIN` durability
  behavior are unchanged.
- No-WAL builds continue to persist through dual snapshot banks, with weaker
  per-mutation durability than WAL-enabled builds. The no-WAL on-media offsets
  changed because the former unused WAL reservation was removed.

## [1.5.0] - 2026-07-27

### Added

- Added deterministic mutation-admission checks before WAL commit and the
  `LOX_ERR_INDETERMINATE` result for storage failures whose durability outcome
  cannot be determined.
- Added storage-faulted handle behavior after indeterminate mutation failures.
- Added detailed preflight storage-layout reporting based on the same layout
  calculation used by initialization.
- Added focused persistence-format, timestamp-width, mutation-atomicity,
  schema-transition, allocator, corruption, and failure-injection coverage.

### Changed

- Redesigned WAL mutation persistence as append-only records without per-entry
  header rewrites; legacy v1.4.5 WAL and snapshot formats remain readable and
  are upgraded through the supported recovery path.
- Persisted timestamps and KV expiration values at a fixed 64-bit width while
  retaining compatibility with legacy 32-bit persisted values.
- Moved operation scratch/staging use into initialization-owned memory so
  normal operations perform no allocator calls after successful initialization.
- Normalized RAM splits for disabled engines and shared the RAM/storage layout
  calculation between preflight and initialization.
- Kept `FOOTPRINT_MIN` WAL-enabled and aligned its profile and size gates with
  that durable contract.
- Restricted schema-version transitions to physically identical schemas;
  incompatible layouts fail before callbacks, WAL writes, or version changes.
- Hardened lock creation, cleanup, callback-outside-lock, and deinitialization
  lifecycle behavior.
- Made core cppcheck, clang-tidy, coverage, compiler-matrix, package, profile,
  and footprint correctness gates blocking where applicable.
- Synchronized the distributed Arduino benchmark sources with the canonical
  implementation and added a bundle-integrity gate.

### Fixed

- Corrected GitHub Actions static-analysis artifact paths and made the macOS
  build avoid the Linux-only GNU linker wrapping test.
- Corrected WAL corruption regression coverage so it damages a real append
  record before crash-style recovery, without leaking the abandoned test heap.
- Rejected exhausted KV slot searches before reading undefined lookup outputs.

### Compatibility

- Existing public enum numeric values are unchanged;
  `LOX_ERR_INDETERMINATE = -15` is appended after `LOX_ERR_MODIFIED`.
- Existing C and C++ source consumers remain supported, and CMake package
  compatibility accepts 1.5 consumers while retaining same-major matching.
- `lox_t` and `lox_schema_t` opaque storage and alignment remain unchanged and
  sufficient for the current implementation.
- `lox_preflight_report_t` gained additive layout fields; consumers using that
  structure must be recompiled and should not assume binary layout compatibility
  with a v1.4.5 object.

## [1.4.5] - 2026-07-17

### Changed

- Package/config hotfix:
  - set the installed `loxdb::loxdb` import location directly in the package
    config so detached consumers can resolve the library without relying on the
    exported target file's config-specific location fields.
  - kept the package import compatible with `Debug` consumers in the detached
    consumer gate.

- Release metadata alignment:
  - `project(loxdb VERSION ...)`, `library.json`, and `library.properties`
    aligned to the release version.
  - `README.md` current release line updated for the next tag.
  - `docs/internal/release-notes.md` draft updated for the next release.

## [1.4.4] - 2026-07-17

### Changed

- Package/config hotfix:
  - fixed installed-package consumers so `Debug` consumers resolve `loxdb::loxdb`
    correctly from the exported package config.
  - kept the installed export compatible with detached package consumers across
    single-config build trees.

- Release metadata alignment:
  - `project(loxdb VERSION ...)`, `library.json`, and `library.properties`
    aligned to the release version.
  - `README.md` current release line updated for the next tag.
  - `docs/internal/release-notes.md` draft updated for the next release.

## [1.4.3] - 2026-07-16

### Added

- Phase 05 build/package/consumer verification:
  - detached installed-package C consumer
  - detached installed-package C++ consumer
  - installed-package version mismatch gate
  - installed header/package config mismatch gate
  - release metadata consistency gate
- Phase 05 documentation truth cleanup:
  - `docs/API_REFERENCE.md`
  - `docs/COOKBOOK.md`
  - `docs/EVIDENCE_MATRIX.md`

- Deterministic startup feasibility API:
  - new `lox_preflight(const lox_cfg_t*, lox_preflight_report_t*)` in core API.
  - new `lox_preflight_report_t` with RAM split and storage layout feasibility fields.
  - preflight sizing mirrors durable layout math used by bootstrap path.
- C++ wrapper startup-gating support:
  - `loxdb::cpp::preflight(...)`
  - `loxdb::cpp::Database::preflight(...)`
- New tests:
  - `tests/test_preflight.c`
  - extended C++ wrapper coverage for preflight flow in `tests/test_cpp_wrapper.cpp`.
- Documentation additions:
  - developer/startup/limits troubleshooting skeletons completed and linked.
  - per-change execution gate (`docs/internal/CHANGE_CYCLE_CHECKLIST.md`).

- Free-tier core additions (MIT):
  - `lox_selfcheck()` API + runtime structural checks for KV/TS/REL/WAL.
  - WCET package:
    - `include/lox_wcet.h` compile-time bound macros,
    - `docs/WCET_ANALYSIS.md` methodology and per-API formulas,
    - `tests/test_wcet_bounds.c` verification coverage.
  - TS logarithmic retention support:
    - new policy constant `LOX_TS_POLICY_LOG_RETAIN`,
    - extended registration API `lox_ts_register_ex(...)`,
    - stream-level log-retain config (`zones`/`zone_pct`),
    - dedicated tests in `tests/test_ts_log_retain.c`.
  - New self-check coverage in `tests/test_selfcheck.c`.

### Changed

- Release/process wording now distinguishes `VERIFIED`, `VERIFIED WITH DEFINED LIMITS`,
  `NOT VERIFIED`, and `INCOMPLETE` for hardware and packaging claims.
- Build/test wiring:
  - registered `test_selfcheck`, `test_wcet_bounds`, and `test_ts_log_retain` in CMake.
  - added dedicated `lox_ts_log_retain` test library target so default TS policy behavior remains unchanged.

## [1.4.2] - 2026-07-16

### Changed

- Release packaging clarity:
  - release assets renamed to `loxdb-sdk-vX.Y.Z-<platform>` to make SDK/binary intent explicit.
  - added explicit `loxdb-source-vX.Y.Z.zip` source distribution asset.
  - source distribution is validated in release workflow (inventory + configure/build/test).
- Version metadata alignment:
  - `project(loxdb VERSION ...)`, `library.json`, and `library.properties` aligned to the release version.
- CMake package install:
  - SDK bundles include `loxdbConfig.cmake`, `loxdbConfigVersion.cmake`, and `loxdbTargets.cmake` for `find_package(loxdb CONFIG REQUIRED)`.

## [1.3.6] - 2026-04-22

### Added

- Offline verifier quality package:
  - deep decode passes for KV/TS/REL pages with warning surfacing.
  - WAL semantic summary counters (including orphaned TXN markers).
  - strict `--check` mode for CI gating.
- New coverage:
  - `tests/test_offline_verifier.c` extended with corruption/recovery/JSON/check-flag scenarios.
  - `tests/test_capacity_estimator_model.c` for capacity-model consistency checks.
  - `tests/test_safety_invariants.c` for safety-critical invariants (magic clear, WAL replay boundary, superblock switch, null-handle contract).
- Certification readiness artifacts:
  - `docs/SAFETY_READINESS.md`
  - `scripts/run_static_analysis.sh`

### Changed

- `tools/lox_capacity_estimator.html` rewritten as single-file real-time planner:
  - preset-driven inputs (FOOTPRINT_MIN, CORE_MIN, CORE_WAL, CORE_PERF, CORE_HIMEM, Custom),
  - RAM/storage/wear outputs,
  - CMake define snippet generation,
  - formula comments tied to source files.
- `docs/PROGRAMMER_MANUAL.md` expanded with Capacity Planning section.
- CI workflow enriched with:
  - verifier integration smoke step (Linux build lane),
  - non-blocking static-analysis job with artifact upload.

### Fixed

- Windows subprocess quoting and WAL entry construction in `tests/test_offline_verifier.c`:
  - stable verifier invocation on Windows (`cmd /c` quoting),
  - orphaned TXN WAL test now writes valid `data_len` in entry header.

## [1.3.5] - 2026-04-22

### Changed

- Release workflow/platform hardening:
  - publish job now explicitly depends on sanitizer gate and build jobs.
  - macOS release artifact label aligned to `macos-arm64`.
- Header-level KV iteration contract clarified as weakly-consistent under concurrent mutation.
- Programmer and thread-safety docs updated with explicit REL mutation semantics and lock/copy behavior notes.

### Fixed

- REL consistency and error semantics:
  - `lox_rel_find` now returns `LOX_ERR_MODIFIED` (not `LOX_ERR_INVALID`) when table mutation is detected after callback re-lock.
  - `lox_rel_iter` now captures `mutation_seq` snapshot and returns `LOX_ERR_MODIFIED` when concurrent mutation is detected.
  - Added regression coverage in `tests/test_rel.c` for both `rel_find` and `rel_iter` concurrent mutation detection.
- REL schema guard:
  - `lox_schema_seal` now rejects rows larger than `LOX_REL_ROW_SCRATCH_MAX` with `LOX_ERR_OVERFLOW`.
  - Added regression test `rel_schema_seal_rejects_oversized_row`.
- TS test-suite stabilization for sanitizer/release profiles:
  - fixed `test_ts` collector overflow for capacities above 256 samples.
  - made `test_ts_reject` and `test_ts_downsample` capacity-aware and RAM-only in setup to avoid storage-path false failures.
  - updated downsample tests to avoid hardcoded query bounds and fixed-size output buffers.
- macOS release build compatibility:
  - fixed footprint baseline linker map flag on Apple toolchain (`-Wl,-map,...` instead of GNU `-Map` form).
  - `tests/check_footprint_min_size.cmake` now falls back to parsing `size -m` output when `size -A` is unavailable on macOS.

## [1.3.0] - 2026-04-21

### Added

- Real-data integration coverage on host (`tests/test_realdata_integration.c`) including KV/TS/REL flows, JSON wrapper, import/export, TXN, and reopen recovery assertions.
- ESP32-S3 `run_real` smoke command in `bench/loxdb_esp32_s3_bench_head` for on-device validation of realistic end-to-end data paths.
- C++ wrapper coverage extension in `tests/test_cpp_wrapper.cpp` for practical KV/TS/REL wrapper usage.

### Changed

- ESP32 benchmark documentation expanded with:
  - WAL sync mode decision table (`SYNC_ALWAYS` vs `SYNC_FLUSH_ONLY`) with measured ESP32 values.
  - POSIX-vs-ESP32 interpretation guidance and follow-up notes for benchmark fidelity.
- Bench sources synchronized for ESP32 harness (`loxdb`, JSON, and import/export modules) to keep local bench build behavior aligned with core sources.
- Release hardening:
  - added macOS CI/release presets (`ci-debug-macos`, `release-macos`) and workflow matrix coverage.
  - release workflow now includes mandatory Linux ASan/UBSan sanitizer gate before packaging.
  - release page body now reads from `CHANGELOG.md` (`body_path`) instead of auto-generated commit-only notes.

### Fixed

- KV JSON import/export TTL sentinel handling:
  - non-expiring keys exported via KV iter sentinel (`UINT32_MAX`) are now normalized to `ttl=0` before JSON encoding.
  - prevents immediate expiry after import for persistent keys.
- KV admission performance:
  - `lox_admit_kv_set` now uses O(1) `live_value_bytes` accounting instead of O(n) bucket scan for compact-availability calculation.
  - verified on ESP32-S3 hardware with reduced `admit_kv_set` latency in `run_real`.
- REL/TS mutation/admission semantics hardening:
  - dedicated `LOX_ERR_MODIFIED` behavior for TS mutation detection in query flows.
  - deterministic budget signaling consistency in REL admission.

### Repository Hygiene

- Removed tracked ESP32 build artifacts under `bench/loxdb_esp32_s3_bench_head/build_sync_flush_only/` and added ignore rule to keep binary outputs out of git history.

## [1.2.0] - 2026-04-20

### Added

- Optional wrapper and backend adapter documentation in README.
- Automated GitHub Release workflow on tag push (`v*`).

### Fixed

- UBSAN misalignment issues in TS/REL and WAL-related paths.
- ASAN leak paths in multiple recovery/reopen test flows.

## [1.1.0] - 2026-04-12

### Added

- Read-only diagnostics and admission APIs.
- Managed stress baseline tooling and threshold recommendation scripts.

### Changed

- Preset-driven CI/release testing strategy (`ci-debug-*`, `release-*`).

## [1.0.0] - 2026-04-01

### Added

- Initial public release with KV, TS, and REL engines.
- WAL durability core and recovery flow.
- C and C++ wrapper surfaces.
