#!/usr/bin/env python3
"""Compile core profiles with -fstack-usage and enforce measured frame limits."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys


PROFILES = {
    "footprint_min": (["LOX_PROFILE_FOOTPRINT_MIN=1", "LOX_ENABLE_TS=0", "LOX_ENABLE_REL=0"], 2688),
    "core_min": (["LOX_PROFILE_CORE_MIN=1", "LOX_THREAD_SAFE=1"], 4352),
    "core_wal": (["LOX_PROFILE_CORE_WAL=1", "LOX_THREAD_SAFE=1"], 4352),
    "core_perf": (["LOX_PROFILE_CORE_PERF=1", "LOX_THREAD_SAFE=1"], 6144),
    "core_himem": (["LOX_PROFILE_CORE_HIMEM=1", "LOX_THREAD_SAFE=1"], 12288),
}
SOURCES = ("loxdb.c", "lox_kv.c", "lox_ts.c", "lox_rel.c", "lox_wal.c", "lox_crc.c")


def parse_usage(path: pathlib.Path) -> list[tuple[int, str]]:
    records: list[tuple[int, str]] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        fields = line.split("\t")
        if len(fields) >= 2:
            try:
                records.append((int(fields[1]), fields[0]))
            except ValueError:
                pass
    return records


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source-dir", required=True, type=pathlib.Path)
    parser.add_argument("--work-dir", required=True, type=pathlib.Path)
    args = parser.parse_args()

    source_dir = args.source_dir.resolve()
    work_dir = args.work_dir.resolve()
    if work_dir.exists():
        shutil.rmtree(work_dir)
    work_dir.mkdir(parents=True)

    failed = False
    for profile, (definitions, limit) in PROFILES.items():
        profile_dir = work_dir / profile
        profile_dir.mkdir()
        records: list[tuple[int, str]] = []
        for source_name in SOURCES:
            source = source_dir / "src" / source_name
            output = profile_dir / (source.stem + ".o")
            command = [
                args.compiler,
                "-std=c99",
                "-O2",
                "-fstack-usage",
                *(f"-D{definition}" for definition in definitions),
                f"-I{source_dir / 'include'}",
                f"-I{source_dir / 'src'}",
                f"-I{source_dir / 'port' / 'ram'}",
                f"-I{source_dir / 'port' / 'posix'}",
                "-c",
                str(source),
                "-o",
                str(output),
            ]
            subprocess.run(command, check=True)
            usage_files = list(profile_dir.glob(source.stem + "*.su"))
            if not usage_files:
                raise RuntimeError(f"compiler produced no stack-usage file for {source_name}")
            for usage_file in usage_files:
                records.extend(parse_usage(usage_file))

        if not records:
            raise RuntimeError(f"no stack-usage records for {profile}")
        maximum, symbol = max(records)
        print(f"{profile}: {maximum} bytes ({symbol}), limit {limit}")
        if maximum > limit:
            print(f"ERROR: {profile} stack frame exceeds {limit} bytes", file=sys.stderr)
            failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
