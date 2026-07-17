# Release Notes Draft (Next Release)

## Title

`loxdb v1.4.5`

## Release text (GitHub Release body)

This release is a package hotfix for detached consumers and release metadata
alignment.

Highlights:

- Fixed detached installed-package consumers by setting the installed
  `loxdb::loxdb` import location directly in the package config.
- Updated release metadata:
  - `CMakeLists.txt`
  - `library.json`
  - `library.properties`
  - `README.md`
  - `CHANGELOG.md`
  - `docs/internal/release-notes.md`

Validation summary:

- Detached consumer and metadata gates were verified in the current build tree.
- Full downstream publish verification remains tied to the release workflow.

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
