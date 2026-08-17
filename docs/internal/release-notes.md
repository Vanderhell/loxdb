# Release Notes Draft (Next Release)

## Title

`loxdb v1.5.3`

## Release text (GitHub Release body)

This release prepares loxdb for its first Arduino Library Manager and
PlatformIO Registry publication, restricted to the verified ESP32 Arduino
scope.

Highlights:

- `lox.h` now has its own C++ linkage guard, so Arduino sketches can call the
  C implementation without C++ name-mangling mismatches.
- Forwarding headers in `src/` expose the canonical public headers from
  `include/` to the Arduino build system without changing the CMake layout.
- `examples/BasicKV/BasicKV.ino` verifies RAM-backed KV use from an ESP32
  Arduino sketch.
- Arduino lint, an ESP32-S3 Arduino compile, PlatformIO package packing, and
  a PlatformIO ESP32-S3 consumer compile are CI gates.
- The CMake SDK now installs `lox_wcet.h` with the other public headers.

The Arduino and PlatformIO package scope is deliberately limited to Arduino
on ESP32/espressif32. ESP-IDF and persistent ESP32-port integration are not
claimed by this registry release.

## Contract links

- `README.md`
- `CHANGELOG.md`
- `docs/API_REFERENCE.md`
- `docs/PROGRAMMER_MANUAL.md`
- `docs/PROFILES.md`
- `docs/EVIDENCE_MATRIX.md`
