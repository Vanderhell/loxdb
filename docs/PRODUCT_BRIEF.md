# microdb Product Brief

## One-Line Positioning
microdb is a deterministic durable storage core for MCU/embedded systems with power-fail-safe KV/TS/REL persistence.

## Target Users
- firmware teams building persistent edge devices
- MCU products that need predictable memory behavior
- embedded products requiring recoverable storage after reset/power loss

## Core Value
- single allocation model at init
- bounded, profile-driven resource behavior
- WAL-backed durability and reopen recovery
- compact C API surface for direct firmware integration

## Product Scope
- KV for config/state
- TS for telemetry rings
- REL for small indexed tables
- profile-oriented operation: `deterministic`, `balanced`, `stress`

## Non-Goals
- SQL query engine
- dynamic optimizer/planner behavior
- large-scale analytics datastore

## Trust Assets
- fail-code contract: `docs/FAIL_CODE_CONTRACT.md`
- profile guarantees and limits: `docs/PROFILE_GUARANTEES.md`
- hard verdict report: `docs/results/hard_verdict_20260412.md`
- matrix + soak results: `docs/results/`

## Adoption Path
1. integrate with RAM-only mode
2. switch to persistent storage HAL
3. choose profile and validate with matrix/soak
4. lock product-level SLOs using your workload
