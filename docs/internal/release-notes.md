# Release Notes Draft (Next Release)

## Title

`loxdb v1.5.0`

## Release text (GitHub Release body)

This release hardens persistence, failure semantics, initialization, schema
transitions, packaging, and CI verification.

Highlights:

- Append-only WAL mutation records with legacy WAL and snapshot readability.
- Fixed-width persisted timestamps and expiration values with legacy decoding.
- No allocator calls in normal operations after successful initialization.
- Deterministic mutation admission, indeterminate failure reporting, and
  storage-faulted handles.
- Shared preflight/init layout calculation, normalized RAM splits, and a
  WAL-enabled `FOOTPRINT_MIN` profile.
- Physically identical schema-version transitions only.
- Blocking static-analysis, coverage, package, profile, and compiler gates.
- Synchronized Arduino source bundles with a regression gate.

Validation summary:

- Release metadata, package/install, detached consumer, strict C99, bundle, and
  supported local build/test gates must pass before tagging.
- Platform-specific and sanitizer verification remains tied to the configured
  CI lanes.

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
