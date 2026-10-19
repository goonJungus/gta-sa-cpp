#!/usr/bin/env python3
"""Regenerate the content inventory at the end of ROADMAP.md from the retail data files.

usage: gen_inventory.py GAME_DATA_DIR GTA_REVERSED_GAME_SA_DIR
Existing [x]/[~] marks are kept; new entries start as [ ]. Weight 0 = tracked, not counted.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
START, END = "<!-- inventory:start -->", "<!-- inventory:end -->"


def rd(p):
    return pathlib.Path(p).read_text(encoding="latin1").replace("\r", "")


def ide(path, idx):
    out = []
    for l in rd(path).split("\n"):
        t = [x for x in re.split(r"[,\s]+", l.strip()) if x]
        if len(t) > idx and t[0].isdigit():
            out.append(t[idx])
    return out


def enum(path, prefix):
    names = []
    for l in rd(path).split("\n"):
        m = re.match(r"\s*(%s\w+)\s*(=|,|$)" % prefix, l)
        if m:
            names.append(m.group(1))
    return names


def main() -> int:
    data, src = sys.argv[1], sys.argv[2]
    road = ROOT / "ROADMAP.md"
    text = road.read_text(encoding="utf-8")
    old = {}
    for l in text.splitlines():
        m = re.match(r"^- \[([x~ ])\] (Inventory \S+: \S+)", l)
        if m:
            old[m.group(2)] = m.group(1)
    rows = []

    def add(kind, name, what, w):
        key = f"Inventory {kind}: {name}"
        mark = old.get(key, "x" if kind == "weapon" and n not in ("ROCKET_HS", "FREEFALL_BOMB") else " ")
        rows.append(f"- [{mark}] {key} - {what} (w {w})")

    for n in sorted(set(ide(data + "/vehicles.ide", 1))):
        add("vehicle", n, "model, handling, damage parts, audio, spawn, drive", 1)
    for n in sorted(set(ide(data + "/peds.ide", 1))):
        add("ped", n, "model, anim group, stats, voice, spawn", 0)
    for l in rd(data + "/weapon.dat").split("\n"):
        if l.startswith("$"):
            add("weapon", l.split()[1], "weapon.dat entry: model, anims, fire, damage, ammo", 1)
    for l in rd(data + "/pedstats.dat").split("\n"):
        if l.strip() and l[0] not in "#;":
            add("pedstat", l.split()[0], "ped stat profile and its behaviour", 0)
    for l in rd(data + "/animgrp.dat").split("\n"):
        l = l.strip()
        if l and l[0] not in "#;" and "," not in l and not l.isdigit() and l != "end":
            add("animgroup", l, "anim group and its blend/walk rules", 1)
    for n in enum(src + "/Enums/eCheats.h", "CHEAT_"):
        add("cheat", n, "cheat code works as retail", 1)
    for n in enum(src + "/Enums/eStats.h", "STAT_"):
        add("stat", n, "statistic tracked, saved and shown", 0)

    def first(path, skip=("end", "endignore", "exclusive", "hide", "cuts", "endexclusive", "//"), n=0, minlen=2):
        out = []
        for l in rd(path).split("\n"):
            l = l.split("#")[0].split(";")[0].strip()
            t = [x for x in re.split(r"[,\s]+", l) if x]
            if len(t) >= minlen and re.match(r"[A-Za-z]\w*$", t[0]) and t[0].lower() not in skip and not t[0].isdigit():
                out.append(t[n])
        return out

    for n in sorted(set(first(data + "/clothes.dat", n=0))):
        add("clothing", n, "clothes.dat item: model, texture, wardrobe slot, shop price", 0)
    for n in sorted(set(first(data + "/melee.dat", n=0))):
        add("melee", n, "melee.dat combo/move: anim, damage, hit reaction", 0)
    for n in sorted(set(first(data + "/info.zon", n=0, skip=("zone", "end", "info")))):
        add("zone", n, "info.zon area: name text, gang/population, radar and map", 0)
    for n in sorted(set(first(data + "/cargrp.dat", n=0))):
        add("cargroup", n, "cargrp.dat traffic group", 0)
    for n in sorted(set(first(data + "/pedgrp.dat", n=0))):
        add("pedgroup", n, "pedgrp.dat population group", 0)
    for n in sorted(set(first(data + "/procobj.dat", n=0))):
        add("procobj", n, "procobj.dat procedurally placed object on surface", 0)
    for n in sorted(set(first(data + "/furnitur.dat", n=0))):
        add("furniture", n, "furnitur.dat interior furniture set", 0)
    for n in sorted(set(first(data + "/carmods.dat", n=0))):
        add("carmods", n, "carmods.dat vehicle upgrade list", 0)
    for f in sorted(pathlib.Path(data + "/Decision").iterdir()):
        if not f.name.startswith("._") and f.is_file():
            add("decision", f.name, "decision maker / ped event file", 0)
    for f in sorted(pathlib.Path(data + "/Paths").iterdir()):
        if not f.name.startswith("._") and f.name != "PaxHeader":
            add("paths", f.name, "path node file: ped and car nodes loaded and used", 0)
    for f in sorted(pathlib.Path(data + "/maps").rglob("*")):
        if f.is_file() and not f.name.startswith("._") and "PaxHeader" not in f.parts:
            add("map", f.relative_to(data + "/maps").as_posix(), "map file: placements, LODs, interiors, collisions, streaming", 0)
    ops = {}
    for l in rd(src + "/Enums/eScriptCommands.h").split("\n"):
        m = re.match(r"\s*COMMAND_\w+\s*=\s*0x([0-9A-Fa-f]+)", l)
        if m:
            ops.setdefault(int(m.group(1), 16) >> 8, []).append(m.group(1))
    for hi in sorted(ops):
        add("opcodes", f"{hi << 8:04X}-{(hi << 8) + 255:04X}", f"{len(ops[hi])} script commands in this block implemented as retail", 2)
    block = "\n".join([START, "## Content inventory (generated by scripts/gen_inventory.py)", *rows, END])
    if START in text:
        text = re.sub(re.escape(START) + ".*?" + re.escape(END), lambda m: block, text, flags=re.S)
    else:
        text = text.rstrip("\n") + "\n\n" + block + "\n"
    road.write_text(text, encoding="utf-8", newline="\n")
    print(len(rows), "inventory lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
