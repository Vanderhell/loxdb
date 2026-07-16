# Release Notes Draft (Next Release)

## Title

`loxdb v1.4.2`

## Release text (GitHub Release body)

This release expands the free MIT core with package-consumer verification,
metadata consistency checks, and documentation truth cleanup.

Highlights:

- Added detached installed-package consumer tests:
  - C consumer
  - C++ consumer
  - version mismatch configure-fail gate
  - installed header config mismatch compile-fail gate
- Added release metadata consistency checks:
  - `CMakeLists.txt`
  - `library.json`
  - `library.properties`
  - `CHANGELOG.md`
  - `docs/internal/release-notes.md`
- Added docs truth cleanup:
  - API reference
  - cookbook
  - evidence matrix

Validation summary:

- Installed consumer and metadata gates were verified in the current build tree.
- GCC/Clang strict C99 and ARM/ESP-IDF compile gates remain NOT VERIFIED here
  unless a concrete run artifact is linked in `docs/EVIDENCE_MATRIX.md`.

## Contract links

- `README.md`
- `CHANGELOG.md`
- `RELEASE_LOG.md`
- `docs/API_REFERENCE.md`
- `docs/COOKBOOK.md`
- `docs/EVIDENCE_MATRIX.md`
- `docs/OFFLINE_VERIFIER.md`
- `docs/PROFILE_GUARANTEES.md`

## Repository topics reference

See `docs/repository-topics.md`.
