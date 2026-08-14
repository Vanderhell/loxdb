# Persistent Byte Order

## Current contract

WAL format v2 (`0x00020000`) and snapshot format v3 (`0x00030000`), including
their supported legacy versions, encode multi-byte integers in the native byte
order of the writing host. The implementation uses `memcpy` for 16- and 32-bit
values; 64-bit values are stored as low and high native-order 32-bit halves.

Consequently, existing media is supported only on a host with the same byte
order. Little-endian media is not portable to a big-endian host, or conversely.
The magic and version fields use the same native encoding, so opposite-endian
legacy media has no reliable, self-identifying byte-order marker. LOXDB does not
guess from byte-swapped magic values.

CRCs cover the bytes exactly as stored. A decoder must establish the format and
byte order before interpreting a stored CRC; changing integer encoding also
changes the CRC input.

## Serialized-field inventory

The native-order contract applies to these integer fields:

- WAL header: magic, version, entry count, block sequence, and CRC (`u32`).
- WAL entry header: magic and sequence (`u32`), payload length (`u16`), and CRC
  (`u32`). Engine and operation identifiers are single bytes.
- Superblock: magic, snapshot version, WAL version, generation, active bank,
  and header CRC (`u32`).
- Snapshot page header: magic, version, generation, payload length, entry
  count, payload CRC, and header CRC (`u32`).
- KV snapshot and WAL payloads: value lengths and transaction identifiers
  (`u32`), plus expiration values represented by one legacy `u32` or two
  current `u32` halves.
- TS snapshot and WAL payloads: raw value size, sample counts, policy metadata,
  and transaction fields (`u32`), plus timestamps represented by two `u32`
  halves.
- REL snapshot and WAL payloads: schema version (`u16`); maximum rows, row
  size, column count, index column, live/row count, column sizes, transaction
  fields, and WAL row sizes (`u32`).

Names, KV keys and values, strings, and blobs are opaque byte sequences. Current
TS sample values and REL scalar cells are copied as raw in-memory bytes, so
cross-endian portability requires defining their scalar representation too; a
framing-only conversion would be incomplete.

The offline verifier (`tools/lox_verify.c`) uses the same native-order decoding
contract and therefore has the same host/media restriction.

## Versioned canonical migration plan

Canonical byte order must be introduced only as a new, atomic on-media format:

1. Reserve WAL v3 (`0x00030000`) and snapshot v4 (`0x00040000`) for canonical
   little-endian encoding. Implement explicit shift-based LE readers and writers;
   do not rely on packed structs or host representation.
2. Select the decoder from literal stored magic/version bytes. New versions are
   always LE. Existing v1/v2 WAL and v2/v3 snapshots remain readable only when
   their native encoding matches the running host, preserving current same-host
   upgrade behavior without heuristic opposite-endian detection.
3. Define every typed payload: TS integers/floats and REL scalar columns use
   canonical LE (with floating-point representation explicitly constrained);
   STR/BLOB and KV values remain opaque. REL conversion uses the persisted
   schema type and width.
4. Write a generation entirely in the new versions. Never combine a canonical
   superblock/snapshot with a legacy WAL, or the reverse. Successful legacy
   recovery is rewritten during the normal flush/compaction generation switch.
5. Update the offline verifier to dispatch by version and report legacy native
   byte order explicitly.
6. Gate the migration with literal-byte golden tests, round trips on both byte
   orders (native or emulated), legacy same-endian recovery, rejection of
   unsupported swapped legacy media, CRC vectors, interrupted migration, and
   mixed-version rejection.

Until all six parts land together, the supported contract remains same-byte-order
media only. This avoids claiming portability while typed payloads or recovery
tools still depend on host representation.
