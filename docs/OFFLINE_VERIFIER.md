# Offline Verifier (`microdb_verify`)

`microdb_verify` is a read-only verifier for a persisted microdb storage image.

## What it checks

- superblock A/B validity (magic/version/header CRC/active bank)
- bank A/B page headers (KV/TS/REL magic/version/header CRC)
- bank A/B payload CRC for KV/TS/REL pages
- WAL header validity and WAL entry-chain integrity
- recovery root selection (`superblock` vs `bank_scan` fallback)

## Output

- human-readable text by default
- JSON with `--json`
- includes verdict, recovery reason, selected bank/generation, WAL summary, and layout metadata

## Exit codes

- `0` = valid/recoverable image (`ok`, `ok_with_wal_header_reset`, `ok_with_wal_tail_truncation`)
- `1` = usage error
- `2` = I/O error
- `3` = invalid verifier config (RAM/split/erase does not match image geometry)
- `4` = unrecoverable corruption
- `5` = uninitialized image (cold-start state)

On failure, stderr uses a stable format:

- `<operation> failed: <VERIFY_CODE_NAME> (<numeric_code>) - <detail>`

## Usage

```bash
microdb_verify --image db.bin --json
microdb_verify --image db.bin --ram-kb 32 --kv-pct 40 --ts-pct 40 --rel-pct 20 --erase-size 4096 --json
```

## Notes

- The verifier is read-only and does not mutate the image.
- Geometry is derived from build/runtime profile inputs (`ram_kb`, split, `erase_size`) and image size.
- If these inputs do not match how the image was produced, verifier returns exit code `3`.
