# Evidence Matrix

Status legend:

- VERIFIED
- VERIFIED WITH DEFINED LIMITS
- NOT VERIFIED
- INCOMPLETE

This document separates source truth from claims that depend on a specific test
run, toolchain, or hardware environment.

| Area | Status | Evidence / Notes |
|---|---|---|
| MSVC Debug build | VERIFIED | Current host build and consumer/package CTest gates were exercised in the verified build tree. |
| GNU strict C99 Debug/Release | NOT VERIFIED | Not executed in this environment. |
| Clang strict C99 Debug/Release | NOT VERIFIED | Not executed in this environment. |
| C++17 wrapper consumer | VERIFIED | Detached installed consumer test is wired in CTest. |
| Installed C consumer | VERIFIED | Detached installed consumer test is wired in CTest. |
| Configuration mismatch installed consumer | VERIFIED | Compile-fail gate against installed headers. |
| Package config/version files | VERIFIED | `cmake/loxdbConfig.cmake.in` and generated package version file are wired into install/export. |
| Source-detached consumer test | VERIFIED | Consumer builds run out-of-tree from staged install prefix. |
| ASan/UBSan gate wiring | VERIFIED WITH DEFINED LIMITS | Workflow/preset wiring present; execution depends on the Linux sanitizer lane. |
| Static-analysis gate wiring | VERIFIED WITH DEFINED LIMITS | Workflow/preset wiring present; execution is non-blocking in CI. |
| ARM/ESP-IDF compile gates | NOT VERIFIED | Not executed here; keep claims limited to configured build wiring. |
| Release workflow consistency checks | VERIFIED WITH DEFINED LIMITS | Workflow contains build, test, install, package, source archive, and consumer validation steps. |
| library.json / library.properties / CMake / changelog consistency | VERIFIED | Added metadata consistency test. |

## Notes

- Real hardware results belong in `docs/results/` and should be cited with the
  exact run file.
- If a claim does not have a concrete test or run artifact, treat it as
  NOT VERIFIED.
