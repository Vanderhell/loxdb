# Release Tag and Announcement Template

## Tag Message Template

```text
microdb <version>

Deterministic durable storage core for MCU/embedded systems.

Highlights:
- Desktop full matrix+soak: PASS (deterministic/balanced/stress)
- ESP32 validation: PASS (deterministic/balanced/stress)
- Product contract updated:
  - profile guarantees
  - fail-code contract
  - hard verdict report
```

## Short Announcement Template

```text
Released microdb <version>.

microdb is a deterministic durable storage core for MCU/embedded systems with power-fail-safe KV/TS/REL persistence.

This release includes:
- validated desktop matrix+soak results
- validated ESP32 profile runs
- updated profile guarantees and hard verdict report

Hard scope notes:
- supported/unsupported is documented explicitly
- effective capacity is not target capacity
- stress is not a low-latency profile
- deterministic is the profile for controlled latencies
- reopen and compact are maintenance operations, not normal write latency

Start here:
- README.md
- docs/GETTING_STARTED_5_MIN.md
- docs/PROFILE_GUARANTEES.md
```

## Release Text Guardrails

- do not add marketing filler
- do not claim "enterprise-grade" without hard proof
- do not hide worst-case latencies
- keep text short, factual, and contract-aligned
