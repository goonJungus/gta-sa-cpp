#!/usr/bin/env python3
"""Learn how many arguments each script command takes by reading the compiled main script itself.

Every argument in the compiled script carries a type byte, so a command can be parsed once its
argument count is known. Counts come from candidate sets (typed retail handler signatures, mined
CollectParameters / StoreParameters calls) and the compiled code settles which candidate is right:
the script is parsed linearly with depth-first backtracking, and every opcode keeps one count
for the whole script.

usage: scm_args.py main.scm out.inc [main offset] [candidates.json ...]

writes one `{opcode, args, outputs, returns a condition},` line per settled opcode. Files named
*rev.json / *auto.json (scm_rev.py) hold exact counts, *revmeta.json the output / condition data,
any other file the mined lower bounds of scm_mine.py.

Each candidates file is a JSON map opcode -> list of possible counts, or the mined
opcode -> [collect counts, store counts, uses] form (the sum is taken).
"""
import collections
import json
import struct
import sys

VARIADIC = {0x004F, 0x0913, 0x0A92, 0x0A93, 0x0AB1}   # argument list ends with a zero byte
KNOWN = {0x0000: 0, 0x0001: 1, 0x0002: 1, 0x004C: 1, 0x004D: 1, 0x004E: 0, 0x0050: 1, 0x0051: 0, 0x00C5: 0, 0x00C6: 0,
         0x00D6: 1, 0x00D7: 1, 0x03A4: 1, 0x0871: 18, 0x0872: 18}
for op in list(range(0x0004, 0x004C)) + list(range(0x0058, 0x0094)):
    KNOWN[op] = 2


def arg_len(b, p):
    """Bytes taken by the argument at p (type byte included), or 0 when it is not a valid argument."""
    if p >= len(b):
        return 0
    t = b[p]
    fixed = {1: 5, 2: 3, 3: 3, 4: 2, 5: 3, 6: 5, 7: 7, 8: 7, 9: 9, 0x0A: 3, 0x0B: 3, 0x0C: 7, 0x0D: 7, 0x0F: 17, 0x10: 3, 0x11: 3, 0x12: 7, 0x13: 7}
    if t in fixed:
        return fixed[t]
    if t == 0x0E:
        return 2 + b[p + 1] if p + 1 < len(b) else 0
    return 0


def after_args(b, p, n):
    for _ in range(n):
        l = arg_len(b, p)
        if l == 0:
            return -1
        p += l
    return p


def after_variadic(b, p):
    n = arg_len(b, p)
    if n == 0:
        return -1
    p += n
    while p < len(b) and b[p] != 0:
        n = arg_len(b, p)
        if n == 0:
            return -1
        p += n
    return p + 1


def main():
    data = open(sys.argv[1], 'rb').read()
    start = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0xDAA8
    cand = collections.defaultdict(list)
    exact = {}
    meta = {}
    stores = {}
    for f in [a for a in sys.argv[4:] if a != '-d']:
        if f.endswith('revmeta.json'):
            meta = {int(k): v for k, v in json.load(open(f)).items()}
            continue
        for k, v in json.load(open(f)).items():
            k = int(k)
            if v and isinstance(v[0], list):
                if not (v[0] or v[1]):
                    continue
                lo = sum(v[0]) + sum(v[1])
                stores[k] = sum(v[1])
                v = list(range(lo, lo + 16))   # mined counts are lower bounds
                cand[k] += [n for n in v if n not in cand[k]]
            else:
                cand[k] = v + [n for n in cand[k] if n not in v]
                if f.endswith('rev.json') or f.endswith('auto.json'):
                    exact[k] = v   # typed signatures are exact
    fixed = dict(KNOWN)
    limit = len(data)

    def options(op):
        if op in fixed:
            return [fixed[op]]
        if op in exact:
            return list(exact[op])
        return cand[op] + [n for n in range(0, 21) if n not in cand[op]] if op in cand else list(range(0, 21))

    def look(q, depth):
        """Commands that parse from q, counting only opcodes whose argument count is settled or single."""
        if depth == 0 or q >= limit - 1:
            return 0
        o = struct.unpack_from('<H', data, q)[0] & 0x7FFF
        if o > 0x0AF0:
            return -3
        if o in fixed:
            n = fixed[o]
        elif len(cand.get(o, ())) == 1:
            n = cand[o][0]
        else:
            return 1
        if o in VARIADIC:
            e = after_variadic(data, q + 2)
        elif o == 0x05B6:
            return 1
        else:
            e = after_args(data, q + 2, n)
        if e < 0:
            return -3
        return 1 + look(e, depth - 1)

    occ_cache = {}

    def occurrences(op):
        if op not in occ_cache:
            hits = []
            for pat in (struct.pack('<H', op), struct.pack('<H', op | 0x8000)):
                i = data.find(pat, start)
                while i >= 0 and len(hits) < 600:
                    hits.append(i)
                    i = data.find(pat, i + 1)
            occ_cache[op] = sorted(hits)
        return occ_cache[op]

    rate_cache = {}

    def rate(op, n):
        """Share of the byte positions holding this opcode where n arguments parse into a plausible next opcode."""
        k = (op, n)
        if k not in rate_cache:
            hits = occurrences(op)
            ok = 0
            for i in hits:
                e = after_variadic(data, i + 2) if op in VARIADIC else after_args(data, i + 2, n)
                if e >= 0 and e < limit - 1 and look(e, 5) >= 0:
                    ok += 1
            rate_cache[k] = ok / max(1, len(hits))
        return rate_cache[k]

    def ranked(p, op, opts):
        if len(opts) <= 1 or op == 0x05B6 or op in VARIADIC:
            return opts
        sc = []
        for k, n in enumerate(opts):
            if n == -1:
                sc.append((0, 0, -k, n))
                continue
            e = after_args(data, p + 2, n)
            sc.append((round(rate(op, n), 2) if op not in exact else 0, look(e, 6) if e >= 0 else -9, -k, n))
        sc.sort(reverse=True)
        out = [n for *_, n in sc]
        if op in cand:   # retail-derived candidates first, the rest only when none parses
            out = [n for n in out if n in cand[op]] + [n for n in out if n not in cand[op]]
        return out

    # depth-first over (position, remaining options); fixed[] holds the choices on the current path
    targets = set()
    stack = []        # [pos, op, opts, idx, newly_fixed]
    pos = start
    best = start
    trail = []
    snap = dict(fixed)
    steps = 0
    commands = 0
    while True:
        steps += 1
        if steps > 3_000_000:
            print('step limit', file=sys.stderr)
            break
        advanced = False
        if pos < limit - 1:
            op = struct.unpack_from('<H', data, pos)[0] & 0x7FFF
            if op <= 0x0AF0:
                stack.append([pos, op, ranked(pos, op, options(op) + ([-1] if op == 0x05B6 else [])), 0, False])
            else:
                stack.append([pos, 0x7FFF, [-1], 0, False])
        else:
            break                       # parsed to the end
        while stack:
            top = stack[-1]
            if top is None:
                stack.pop()
                break
            p, op, opts, i, newly = top
            if newly:
                del fixed[op]
                top[4] = False
            q = -1
            while i < len(opts):
                n = opts[i]
                i += 1
                if n == -1:
                    # not code: a debug block the script jumps over; resume where a run of known commands parses
                    z = data.find(b'\0', p + 2)
                    q = -1
                    for c in range(z + 1, min(z + 0x2000, limit - 1)):
                        if look(c, 14) >= 14:
                            q = c
                            break
                elif op == 0x05B6:   # SAVE_STRING_TO_DEBUG_FILE: a raw zero-terminated string
                    z = data.find(b'\0', p + 2)
                    q = z + 1 if z >= 0 else -1
                elif op in VARIADIC:
                    q = after_variadic(data, p + 2)
                else:
                    q = after_args(data, p + 2, n)
                if q < 0:
                    continue
                nxt = struct.unpack_from('<H', data, q)[0] & 0x7FFF if q < limit - 1 else 0
                if nxt > 0x0AF0:
                    q = -1
                    continue
                break
            top[3] = i
            if q >= 0:
                if op in (0x0002, 0x004C, 0x004D, 0x0050) and data[p + 2] == 1:
                    t = struct.unpack_from('<i', data, p + 3)[0]
                    if t > 0:
                        targets.add(t)
                if op not in fixed and n != -1:
                    fixed[op] = n
                    if op == 0x0104 and '-d' in sys.argv:
                        print('fix 0104 =', n, 'at %X' % p, file=sys.stderr)
                    top[4] = True
                pos = q
                advanced = True
                if q > best:
                    best = q
                    trail = [(e[0], e[1]) for e in stack[-14:] if e]
                    snap = dict(fixed)
                break
            stack.pop()
        if not advanced:
            if not stack:
                print('no parse; furthest 0x%X' % best, file=sys.stderr)
                break
    print('furthest bytes:', data[best - 40:best + 16].hex(' '), file=sys.stderr)
    print('trail:', ' '.join('%X:%04X/%s' % (a, o, fixed.get(o)) for a, o in trail), file=sys.stderr)
    print('parsed to 0x%X of 0x%X, %d commands, %d opcodes settled' % (best, limit, len(stack), len(snap)),
          file=sys.stderr)
    with open(sys.argv[2], 'w') as out:
        for op in sorted(snap):
            m = meta.get(op)
            nout = m[1] if m and m[1] >= 0 else stores.get(op, 0)
            out.write('{0x%04X, %d, %d, %d},\n' % (op, snap[op], min(nout, snap[op]), 1 if m and m[2] else 0))


if __name__ == '__main__':
    main()
