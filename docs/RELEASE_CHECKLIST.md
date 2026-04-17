# Release Checklist

Use this checklist before creating a public release tag.

## 1. Validation Gates

- [ ] Desktop full validation passed:
  - `tools/run_full_validation.ps1 -SkipBuild -SkipEsp32`
  - latest summary archived in `docs/results/`
- [ ] ESP validation passed on current firmware:
  - `deterministic`, `balanced`, `stress`
  - logs archived in `docs/results/`
- [ ] Hard verdict updated:
  - `docs/results/hard_verdict_YYYYMMDD.md`
- [ ] Release preset smoke lanes are green for both platforms:
  - `test_backend_managed_stress_smoke`
  - `test_backend_fs_matrix_smoke`
  - `test_optional_backend_strip_gate`
- [ ] Release preset footprint/tiny guards are green:
  - `test_tiny_size_guard`
  - `test_footprint_min_size_gate_release`

## 2. Product Contract Gates

- [ ] Positioning sentence is final:
  - `docs/PRODUCT_POSITIONING.md`
- [ ] Profile guarantees table is updated with latest results:
  - `docs/PROFILE_GUARANTEES.md`
- [ ] Fail-code contract is current:
  - `docs/FAIL_CODE_CONTRACT.md`
- [ ] Supported/unsupported scope is explicit in product docs
- [ ] Statement is explicit: effective capacity is not target capacity
- [ ] Statement is explicit: stress is not a low-latency profile
- [ ] Statement is explicit: deterministic is the profile for controlled latencies
- [ ] Statement is explicit: reopen/compact are maintenance operations, not normal write latency

## 3. Adoption Gates

- [ ] Quick onboarding is valid:
  - `docs/GETTING_STARTED_5_MIN.md`
- [ ] Product brief is current:
  - `docs/PRODUCT_BRIEF.md`
- [ ] README links are correct:
  - `README.md`

## 4. Build and Packaging Gates

- [ ] Target build config is clean (`Debug`/`Release` as intended)
- [ ] Artifacts generated for all promised platforms
- [ ] Checksums generated for each distributable archive
- [ ] Archive names match release notes template
- [ ] Gate mapping to release criteria is complete:
  - Durability/recovery: `test_durability_closure`, `test_resilience`, `test_wal`
  - API contract: `test_api_contract_matrix`, `test_fail_code_contract`
  - Capacity/profiles: `test_profile_core_*`, `test_storage_capacity_profiles`
  - Backend adapters: `test_backend_open`, `test_backend_*_recovery`

## 5. Release Metadata Gates

- [ ] Tag version selected (example: `v1.1.0`)
- [ ] Release notes prepared:
  - `docs/release-notes.md`
- [ ] Repository topics checked:
  - `docs/repository-topics.md`

## 6. Final Go/No-Go

- [ ] No unresolved `FAIL` in latest validation summary
- [ ] No unresolved contract/documentation mismatch
- [ ] Tag + release publish approved
- [ ] No hidden worst-case latency in release text
- [ ] No unsupported claims (`enterprise-grade`, etc.) without hard evidence
- [ ] No marketing filler that weakens technical clarity
