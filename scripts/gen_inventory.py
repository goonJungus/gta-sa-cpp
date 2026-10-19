#!/usr/bin/env python3
"""Regenerate the content inventory at the end of ROADMAP.md from the retail data files.

usage: gen_inventory.py GAME_DATA_DIR GTA_REVERSED_GAME_SA_DIR
Existing [x]/[~] marks are kept; new entries start as [ ]. Weight 0 = tracked, not counted.
"""
import pathlib
import re
import struct
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


def img_entries(path):
    """VER2 IMG directory: [(name, offset_sectors, size_sectors)]."""
    with open(path, "rb") as f:
        head = f.read(8)
        if head[:4] != b"VER2":
            return []
        n = struct.unpack("<I", head[4:])[0]
        raw = f.read(32 * n)
    out = []
    for i in range(n):
        off, size16, size, name = struct.unpack("<IHH24s", raw[32 * i:32 * i + 32])
        out.append((name.split(b"\0")[0].decode("latin1"), off, size16 or size))
    return out


def ifp_names(blob):
    """Animation names inside an ANP3 block."""
    if blob[:4] != b"ANP3":
        return []
    n = struct.unpack("<I", blob[32:36])[0]
    pos, names = 36, []
    for _ in range(n):
        name, nodes, _size, _u = struct.unpack("<24sIII", blob[pos:pos + 36])
        names.append(name.split(b"\0")[0].decode("latin1"))
        pos += 36
        for _j in range(nodes):
            _nm, ty, fr, _bt = struct.unpack("<24sIII", blob[pos:pos + 36])
            pos += 36 + fr * (16 if ty == 4 else 10)
    return names


def gxt_tables(path):
    d = pathlib.Path(path).read_bytes()
    out = []
    if d[4:8] != b"TABL":
        return out
    size = struct.unpack("<I", d[8:12])[0]
    for i in range(size // 12):
        name, off = struct.unpack("<8sI", d[12 + 12 * i:24 + 12 * i])
        name = name.split(b"\0")[0].decode("latin1")
        base = off if name == "MAIN" else off + 8
        out.append((name, base))
    return out


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

    game = str(pathlib.Path(data).parent)
    for img in ("models/gta3.img", "models/gta_int.img", "models/player.img", "models/cutscene.img", "anim/cuts.img"):
        for name, _o, _s in img_entries(game + "/" + img):
            add("asset", img.split("/")[1] + "/" + name, "loaded from the IMG and used as retail", 0)
    anim = game + "/anim/anim.img"
    with open(anim, "rb") as f:
        for name, off, size in img_entries(anim):
            f.seek(off * 2048)
            blob = f.read(size * 2048)
            add("animblock", name, "anim.img block loaded on demand", 0)
            for a in ifp_names(blob):
                add("anim", name + "/" + a, "animation clip plays as retail", 0)
    ped = pathlib.Path(game + "/anim/ped.ifp").read_bytes()
    for a in ifp_names(ped):
        add("anim", "ped.ifp/" + a, "animation clip plays as retail", 0)
    for sub in ("audio/sfx", "audio/streams", "audio/CONFIG"):
        for f in sorted(pathlib.Path(game + "/" + sub).iterdir()):
            if not f.name.startswith("._") and f.name != "PaxHeader":
                add("audio", sub + "/" + f.name, "audio file decoded and played as retail", 0)
    for f in sorted(pathlib.Path(game + "/movies").iterdir()):
        if not f.name.startswith("._") and f.name != "PaxHeader":
            add("movie", f.name, "movie plays as retail", 0)
    for f in sorted(pathlib.Path(game + "/text").glob("*.gxt")):
        if not f.name.startswith("._"):
            for t, _b in gxt_tables(str(f)):
                add("gxt", f.name + "/" + t, "text table loaded, keys resolved, shown as retail", 0)
    for sub in ("models/txd", "models/coll", "models/generic", "models/grass"):
        for f in sorted(pathlib.Path(game + "/" + sub).iterdir()):
            if not f.name.startswith("._") and f.name != "PaxHeader":
                add("asset", sub + "/" + f.name, "file loaded and used as retail", 0)
    for f in sorted(pathlib.Path(game + "/models").glob("*.txd")) + [pathlib.Path(game + "/models/effects.fxp")]:
        add("asset", "models/" + f.name, "file loaded and used as retail", 0)
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
