# TODO

## Current Status (2026-04-17)
- Storage architecture roadmap (core + adapters) is complete:
  - Capability descriptor, backend classes, open-time compatibility gate.
  - Aligned adapter and managed adapter paths with fail-fast validation.
  - Filesystem/block adapter path with explicit sync policies.
  - Optional backend modular packaging + strip-link gate.
  - Backend matrix and recovery coverage across aligned/managed/fs paths.
  - Capacity profiles + estimator tooling.
- Runtime calibration tooling is in place for:
  - Managed stress thresholds (`recommend/apply/finalize` + baseline refresh workflow).
  - FS matrix thresholds (`recommend/apply/finalize` + baseline refresh workflow).

## Next Roadmap (Production Hardening)
1. [todo] Add dedicated fault-injection lanes for fs/block and managed adapters:
   - deterministic write/read/erase/sync fault points
   - expected fail-code and recovery assertions
2. [todo] Add property/fuzz-style API stress suite:
   - randomized operation sequences + invariant checks
   - reproducible seed capture and replay
3. [todo] Add long-soak nightly workflow:
   - 30-60 min endurance lane
   - artifactized runtime + failure snapshots
4. [todo] Add release gate tightening:
   - mandatory fs/managed matrix smoke lanes on release presets
   - explicit checklist mapping test gates to release criteria
5. [todo] Add observability/reporting rollup:
   - single markdown report combining managed + fs baseline trend summary
   - drift warnings when thresholds tighten/loosen beyond policy

## Current Truth (Do Not Relax)
- Core storage contract remains strict:
  - `erase_size > 0`
  - `write_size == 1`
- Adapter paths may broaden media support, but must not relax core durability rules.
