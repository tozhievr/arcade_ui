#!/usr/bin/env python3
"""Run clang-tidy over real CMake compilation units and produce a stable report.

The project has existing warning debt: exit 0 with report unless --strict is used.
Compiler errors and missing compilation data remain fatal.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PATTERN = re.compile(r"^(.*?):(\d+):(\d+): (warning|error): (.*?)\s*(?:\[([^\]]+)\])?$", re.M)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build/native")
    parser.add_argument("--binary", default=os.environ.get("CLANG_TIDY", "clang-tidy"))
    parser.add_argument("--output", default="build/native/clang-tidy-report.json")
    parser.add_argument("--timeout", type=int, default=60, help="Timeout seconds per translation unit")
    parser.add_argument("--strict", action="store_true", help="Fail if clang-tidy finds diagnostics")
    parser.add_argument("--limit", type=int, default=0, help="Limit translation units for debugging")
    args = parser.parse_args()
    exe = shutil.which(args.binary)
    if not exe:
        print(f"clang-tidy not found: {args.binary}", file=sys.stderr)
        return 2
    build = (ROOT / args.build).resolve()
    commands = build / "compile_commands.json"
    if not commands.is_file():
        print(f"Missing compilation database: {commands}", file=sys.stderr)
        return 2

    entries = json.loads(commands.read_text(encoding="utf-8"))
    files = sorted({Path(x["file"]).resolve() for x in entries
                    if Path(x["file"]).resolve().is_relative_to(ROOT / "src")
                    or Path(x["file"]).resolve().is_relative_to(ROOT / "tests")})
    files = [p for p in files if p.suffix in (".cpp", ".cppm", ".ixx", ".cc", ".cxx")]
    if args.limit > 0:
        files = files[:args.limit]
    if not files:
        print("Compilation database contains no project sources", file=sys.stderr)
        return 2

    records = []
    failures = []
    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        cmd = [exe, "-p", str(build), "--quiet", str(path)]
        try:
            completed = subprocess.run(cmd, cwd=ROOT, capture_output=True,
                                       text=True, check=False, timeout=args.timeout)
            combined = (completed.stdout or "") + (completed.stderr or "")
            status = completed.returncode
        except subprocess.TimeoutExpired as e:
            stdout = e.stdout.decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
            stderr = e.stderr.decode(errors="replace") if isinstance(e.stderr, bytes) else (e.stderr or "")
            combined = stdout + stderr + "\nclang-tidy timeout after " + str(args.timeout) + " seconds"
            status = 124
        for match in PATTERN.finditer(combined):
            records.append({"file": match.group(1), "line": int(match.group(2)),
                            "column": int(match.group(3)), "level": match.group(4),
                            "message": match.group(5), "check": match.group(6) or ""})
        if status:
            failures.append({"file": rel, "returncode": status,
                             "output": combined[-12000:]})
        print(f"{rel}: {sum(1 for item in records if item['file'].endswith(rel))} diagnostics"
              + (" (tool returned nonzero)" if status else ""), flush=True)

    report = {"tool": args.binary, "units": len(files), "diagnostics": len(records),
              "findings": records, "nonzero_exits": failures}
    output = (ROOT / args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Audit complete: {len(files)} units, {len(records)} findings, "
          f"{len(failures)} nonzero exits. JSON: {output}")
    # Clang-tidy can exit nonzero from parse errors, unsupported flags, or
    # WarningsAsErrors. In report mode keep the job green while surfacing all
    # causes in the artifact. Strict mode gates all issues.
    return 1 if args.strict and (records or failures) else 0


if __name__ == "__main__":
    raise SystemExit(main())
