#!/usr/bin/env python3
"""Check that every retail decomp class folder and loose function is mapped to a ROADMAP.md item.

usage: coverage.py DECOMP_SRC_DIR   (the Ghidra decomp's src folder)
Rules live in scripts/coverage_map.txt: `regex<TAB>text that must appear in a ROADMAP.md line`.
Exit 1 when something is unmapped or a rule points at text the roadmap does not have.
"""
import os
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    src = pathlib.Path(sys.argv[1])
    road = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    rules = []
    for line in (ROOT / "scripts" / "coverage_map.txt").read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        rx, target = line.split("\t")
        rules.append((re.compile(rx), target))
    bad = [t for _, t in rules if t != "EXCLUDED" and t not in road]
    for t in sorted(set(bad)):
        print("roadmap has no line containing:", t)
    names = sorted(d for d in os.listdir(src) if d != "_global")
    gl = src / "_global"
    if gl.is_dir():
        for f in os.listdir(gl):
            n = re.sub(r"(_[0-9a-f]{8})+\.c$", "", f)
            names.append(n)
    unmapped, counts = set(), {}
    for n in set(names):
        for rx, t in rules:
            if rx.match(n):
                counts[t] = counts.get(t, 0) + 1
                break
        else:
            unmapped.add(n)
    for n in sorted(unmapped):
        print("unmapped:", n)
    print(f"{len(set(names)) - len(unmapped)} mapped, {len(unmapped)} unmapped, {len(bad)} dead rules")
    return 1 if unmapped or bad else 0


if __name__ == "__main__":
    sys.exit(main())
