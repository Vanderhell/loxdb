# Release Checklist

Use this checklist before creating a public release tag.

Status note:

- This file is a release-process checklist, not a claim that every lane has been
  rerun in the current turn.
- Historical verification entries below remain historical unless the current
  build output or `docs/results/` evidence says otherwise.
- Phase 05 package/consumer and metadata gates were verified in
  `build/phase05-win` during this turn.

## 1. Validation Gates

- [x] Historical desktop validation exists in `docs/results/`
  - latest recorded full run: `docs/results/validation_summary_20260419_180500.md`
- [x] Historical ESP validation exists in `docs/results/`
  - latest recorded run: `docs/results/validation_summary_20260419_193234.md`
- [x] Historical hard verdict exists in `docs/results/`
  - latest recorded: `docs/results/hard_verdict_20260419.md`
- [ ] Current Phase 05 consumer/package gates re-run in this environment
  - `test_release_metadata_consistency`
  - `test_source_detached_consumer_c`
  - `test_source_detached_consumer_cpp`
  - `test_installed_package_version_mismatch`
  - `test_installed_package_config_mismatch`
- [ ] Current compiler/sanitizer gates re-run in this environment
  - GCC strict C99 Debug/Release
  - Clang strict C99 Debug/Release
  - ASan/UBSan gate wiring
- [ ] Current ARM/ESP-IDF compile gates re-run in this environment, or else marked `NOT VERIFIED`

## 2. Product Contract Gates

- [x] Positioning sentence exists in the product docs
- [x] Supported/unsupported scope is explicit in product docs
- [x] Capacity and maintenance terminology is separated from normal write latency claims
- [x] Hardware and portability claims are represented in `docs/EVIDENCE_MATRIX.md`

## 3. Adoption Gates

- [x] Quick onboarding exists
- [x] Programmer manual exists
- [x] README links to the current API/cookbook/evidence docs

## 4. Build and Packaging Gates

- [ ] GCC strict C99 Debug/Release build verified
- [ ] Clang strict C99 Debug/Release build verified
- [ ] Installed C consumer verified
- [ ] Installed C++ consumer verified
- [ ] Source-detached consumer verification verified
- [ ] Release archive names and package layout verified against the current release notes template
- [ ] Release workflow consistency checked against the current metadata set

## 5. Release Metadata Gates

- [x] `project(loxdb VERSION ...)` matches `library.json`
- [x] `library.properties` version matches `CMakeLists.txt`
- [x] `CHANGELOG.md` version entry matches `CMakeLists.txt`
- [x] `docs/internal/release-notes.md` matches the current release line
- [x] `library.json` / `library.properties` / CMake / changelog consistency gate is wired

## 6. Final Go/No-Go

- [ ] No unresolved `FAIL` in the current validation summary
- [ ] No unresolved contract/documentation mismatch
- [ ] No unsupported hardware or power-loss claim without concrete evidence
- [ ] Tag and release publish approved only after current phase gates are verified
