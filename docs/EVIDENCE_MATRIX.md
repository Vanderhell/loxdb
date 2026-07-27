# Evidence Matrix

This matrix separates what was executed on the current Windows host from CI
configuration and hardware evidence. CI wiring is not evidence that a lane
executed successfully.

| Area | Evidence class | Current evidence |
|---|---|---|
| MSVC Debug, full CTest, install/package, detached C/C++ consumers, mismatch gates, profiles and footprint | Executed locally | 110/110 CTest entries passed on Windows. |
| MinGW GCC 16.1 Debug/Release | Executed locally | Debug passed 110/110; Release passed 111/111 including the Release footprint gate. |
| Clang 22.1 MinGW-target Debug/Release | Executed locally | Debug passed 110/110; Release passed 111/111 including the Release footprint gate. |
| Core cppcheck | Executed locally and blocking CI | `warning`, `performance`, and `portability` checks pass with zero findings. |
| Core clang-tidy | Executed locally and blocking CI | Analyzer, bugprone, performance, and portability checks pass after genuine findings were fixed. |
| TS timestamp-width and no-WAL recovery patch | Executed locally | Programmatically constructed snapshot/WAL boundary media, 64-bit and legacy TS reopen, exact no-WAL layout/capacity, zero WAL accounting, KV snapshot reopen, and interrupted snapshot fallback pass in every local full CTest lane. |
| Core line coverage | Measured locally and blocking CI | 4216/5017 executable lines, 84.03%; CI enforces 83% over the named core and RAM/POSIX port sources. |
| Linux GCC Debug/Release | Blocking CI configuration | Debug platform matrix and Release compiler matrix run the full CTest suite. Not executed on this Windows host. |
| Linux Clang Debug/Release | Blocking CI configuration | Compiler matrix runs the full CTest suite. Not executed on this Windows host. |
| Linux ASan/UBSan | Blocking CI configuration | Preset builds and runs the full suite with halt-on-error. Local MinGW GCC lacks ASan/UBSan runtimes, and the installed Windows Clang cannot link the MinGW sanitizer target, so this lane was not executed here. |
| macOS Debug | Blocking CI configuration | Platform matrix runs build and full CTest. Not executed on this Windows host. |
| Strict C99, installed consumers, package/install, configuration mismatch, size/profile gates | Blocking tests | These are CTest gates in each full build; their local MSVC results are included above. |
| ESP32 benchmark numbers | In-RAM backend evidence | Measurements in `BENCHMARKS.md` cover target CPU plus the benchmark's RAM flash-like backend; they do not validate physical NOR I/O or power loss. |
| Physical NOR erase/write/sync, brownout recovery, endurance | No physical-hardware evidence in this verification | No claim is made. These require a documented target-media run. |
| ESP32 Arduino bundles | Source integrity executed locally | The canonical-source bundle-integrity gate passes for all three bundled source trees. Arduino compilation, board execution, and physical-media testing were not executed for this patch on the current host. |

The CI workflow is the source of truth for configured gates. A green workflow
run is required before describing a CI lane as executed.
