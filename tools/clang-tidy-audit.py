#!/usr/bin/env python3
"""Run native clang-tidy over all C++ translation units; record every diagnostic."""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DIAGNOSTIC = re.compile(r"^(.+?):(\d+):(\d+):\s+(warning|error):\s+(.*?)(?:\s+\[([^]]+)\])?$", re.M)

def analyze(path: Path, exe: str, build: Path, timeout: int) -> dict:
    relative = path.relative_to(ROOT).as_posix()
    command = [exe, "-p", str(build), "--quiet", str(path)]
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True,
                                text=True, check=False, timeout=timeout)
        output = (result.stdout or "") + (result.stderr or "")
        status = result.returncode
    except subprocess.TimeoutExpired as exc:
        out = exc.stdout.decode(errors="replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        err = exc.stderr.decode(errors="replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
        output = out + err + f"\nclang-tidy timed out after {timeout}s"
        status = 124
    findings = [
        {"file": m.group(1), "line": int(m.group(2)), "column": int(m.group(3)),
         "level": m.group(4), "message": m.group(5), "check": m.group(6) or ""}
        for m in DIAGNOSTIC.finditer(output)
    ]
    return {"file": relative, "status": status, "findings": findings,
            "output": output[-15000:] if status else ""}

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build/native")
    parser.add_argument("--binary", default=os.environ.get("CLANG_TIDY", "clang-tidy"))
    parser.add_argument("--output", default="build/native/clang-tidy-report.json")
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()

    exe = shutil.which(args.binary)
    if exe is None:
        print(f"Cannot locate clang-tidy: {args.binary}", file=sys.stderr)
        return 2
    build = (ROOT / args.build).resolve()
    database = build / "compile_commands.json"
    if not database.is_file():
        print(f"Missing compilation database: {database}", file=sys.stderr)
        return 2
    entries = json.loads(database.read_text(encoding="utf-8"))
    files = sorted({Path(x["file"]).resolve() for x in entries
                    if Path(x["file"]).resolve().is_relative_to(ROOT / "src")
                    or Path(x["file"]).resolve().is_relative_to(ROOT / "tests")})
    files = [p for p in files if p.suffix in {".cpp", ".cc", ".cxx", ".cppm", ".ixx"}]
    if not files:
        print("No C++ translation units in compilation database", file=sys.stderr)
        return 2

    results = []
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        pending = {pool.submit(analyze, path, exe, build, args.timeout): path for path in files}
        for task in as_completed(pending):
            result = task.result()
            results.append(result)
            print(f"{result['file']}: {len(result['findings'])} diagnostics, exit={result['status']}", flush=True)

    results.sort(key=lambda r: r["file"])
    findings = [item for row in results for item in row["findings"]]
    errors = [{"file": row["file"], "returncode": row["status"], "output": row["output"]}
              for row in results if row["status"] != 0]
    report = {"tool": args.binary, "units": len(results), "diagnostics": len(findings),
              "findings": findings, "nonzero_exits": errors}
    destination = (ROOT / args.output).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Audit complete: {len(results)} units, {len(findings)} findings, "
          f"{len(errors)} nonzero exits. JSON: {destination}", flush=True)
    if args.strict and (findings or errors):
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
