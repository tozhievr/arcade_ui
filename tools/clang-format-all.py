#!/usr/bin/env python3
"""Deterministic Arcade UI formatting: --check for CI, --fix for maintainers."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {".cpp", ".cc", ".cxx", ".hpp", ".h", ".cppm", ".ixx"}


def sources() -> list[Path]:
    # Never format compiler-generated files or vendored code.
    return sorted(
        (p for folder in ("src", "tests") for p in (ROOT / folder).rglob("*")
         if p.is_file() and p.suffix in EXTENSIONS),
        key=lambda p: p.relative_to(ROOT).as_posix(),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="Check only, fail on differences (default)")
    mode.add_argument("--fix", action="store_true", help="Format in place")
    parser.add_argument("--binary", default=os.environ.get("CLANG_FORMAT", "clang-format"),
                        help="clang-format executable (CLANG_FORMAT env also supported)")
    args = parser.parse_args()

    binary = shutil.which(args.binary)
    if binary is None:
        print(f"clang-format binary not found: {args.binary}", file=sys.stderr)
        return 2

    files = sources()
    if not files:
        print("No C++ sources found", file=sys.stderr)
        return 2

    # Use one process per file to identify the failing source exactly.
    failures = 0
    for path in files:
        display = path.relative_to(ROOT).as_posix()
        cmd = [binary, "--style=file", "--fallback-style=none"]
        if args.fix:
            cmd += ["-i", str(path)]
        else:
            cmd += ["--dry-run", "--Werror", str(path)]
        result = subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True, check=False)
        if result.returncode:
            failures += 1
            print(f"FAIL {display}", file=sys.stderr)
            if result.stderr:
                print(result.stderr.rstrip(), file=sys.stderr)
            if result.stdout:
                print(result.stdout.rstrip(), file=sys.stderr)
        elif args.fix:
            print(f"Formatted {display}")
    print(f"clang-format: {'fixed' if args.fix else 'checked'} {len(files)} files, "
          f"{failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
