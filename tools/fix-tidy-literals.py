#!/usr/bin/env python3
"""Mechanically normalize numeric literal suffixes, preserving C++ literals and strings."""
from __future__ import annotations
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FLOAT = re.compile(r"(?<![A-Za-z0-9_])((?:[0-9][0-9']*\.[0-9']*|\.[0-9][0-9']*)(?:[eE][+-]?[0-9][0-9']*)?|[0-9][0-9']*[eE][+-]?[0-9][0-9']*)f\b")
UNSIGNED = re.compile(r"(?<![A-Za-z0-9_])((?:0[xX][0-9a-fA-F']+|0[bB][01']+|[0-9][0-9']*))u\b")

def convert_fragment(fragment: str) -> tuple[str, int]:
    fragment, floating = FLOAT.subn(lambda match: match[1] + "F", fragment)
    fragment, unsigned = UNSIGNED.subn(lambda match: match[1] + "U", fragment)
    return fragment, floating + unsigned

def rewrite(source: str) -> tuple[str, int]:
    parts = []
    start = 0
    index = 0
    replacements = 0
    while index < len(source):
        if source.startswith("//", index):
            end = source.find("\n", index + 2)
            end = len(source) if end < 0 else end
        elif source.startswith("/*", index):
            end = source.find("*/", index + 2)
            end = len(source) if end < 0 else end + 2
        elif source.startswith('R"', index):
            opening = source.find("(", index + 2)
            if opening < 0 or opening - index > 20:
                index += 1
                continue
            delimiter = source[index + 2:opening]
            closing = source.find(")" + delimiter + '"', opening + 1)
            if closing < 0:
                index += 1
                continue
            end = closing + len(delimiter) + 2
        elif source[index] in ('"', "'"):
            quote = source[index]
            end = index + 1
            while end < len(source):
                if source[end] == "\\":
                    end += 2
                elif source[end] == quote:
                    end += 1
                    break
                else:
                    end += 1
        else:
            index += 1
            continue
        converted, count = convert_fragment(source[start:index])
        parts.extend((converted, source[index:end]))
        replacements += count
        index = end
        start = end
    converted, count = convert_fragment(source[start:])
    parts.append(converted)
    return "".join(parts), replacements + count

def main() -> int:
    total = 0
    for dirname in ("src", "tests"):
        for path in sorted((ROOT / dirname).rglob("*")):
            if path.suffix not in {".cpp", ".cppm", ".ixx", ".h", ".hpp"}:
                continue
            original = path.read_text(encoding="utf-8")
            updated, count = rewrite(original)
            if count:
                path.write_text(updated, encoding="utf-8")
                total += count
                print(f"{path.relative_to(ROOT)}: {count} suffixes")
    print(f"Fixed {total} lowercase numeric literal suffixes")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
