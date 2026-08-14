# Core Stack-Usage Budget

LOXDB gates compiler-generated stack frames for MCU-relevant core profiles.
The gate compiles the six core translation units with GCC or Clang at `-O2`
and parses `-fstack-usage` output. Host-only tools and optional import/export
modules are intentionally outside the core budget.

The limits are rounded ceilings above the higher GCC 13/Clang 18 measurement
taken on 2026-08-14:

| Profile | Measured maximum | Limit | Maximum function |
|---|---:|---:|---|
| FOOTPRINT_MIN | 2576 B | 2688 B | `lox_storage_bootstrap` (GCC) |
| CORE_MIN | 4224 B | 4352 B | `lox_storage_bootstrap` (GCC) |
| CORE_WAL/default | 4224 B | 4352 B | `lox_storage_bootstrap` (GCC) |
| CORE_PERF | 5760 B | 6144 B | transaction preparation (GCC) |
| CORE_HIMEM | 11392 B | 12288 B | transaction preparation (GCC) |

Compiler versions and optimization choices can move frames slightly, which is
why the limits are rounded rather than equal to a single compiler's result.
Growth beyond a ceiling requires either reducing the frame or deliberately
remeasuring and reviewing the budget.

The public object gates are 8224 bytes for `lox_t` and 912 bytes for
`lox_schema_t`, including alignment padding. MCU applications should normally
place `lox_t` in static/global storage, as shown by the embedded examples,
rather than allocating it in a task stack frame.

Run the gate through CTest on a GCC/Clang build, or directly:

```sh
python3 tests/check_stack_usage.py \
  --compiler clang \
  --source-dir . \
  --work-dir build/stack-usage-gate
```
