#!/usr/bin/env python3
"""Learn the argument counts of the commands only the mission scripts use.

scm_args.py settles the commands of the main script. The mission scripts (the code after the main
script, one block per mission) use many more. Every mission is parsed linearly from its start to the
start of the next one with depth-first backtracking: every opcode keeps one count across all missions,
an unknown opcode tries counts upward from the mined lower bound, a parse is valid only when every
argument has a valid type byte, every opcode is a known command name and the block ends exactly at the
next block.

usage: scm_missions.py main.scm ScmArgs.inc cnt.json eScriptCommands.h out.inc
"""
import json
import re
import struct
import sys

VARIADIC = {0x004F, 0x0913, 0x0A92, 0x0A93, 0x0AB1}


def arg_len(b, p, end):
    if p >= end:
        return 0
    t = b[p]
    fixed = {1: 5, 2: 3, 3: 3, 4: 2, 5: 3, 6: 5, 7: 7, 8: 7, 9: 9, 0x0A: 3, 0x0B: 3, 0x0C: 7, 0x0D: 7, 0x0F: 17, 0x10: 3, 0x11: 3, 0x12: 7, 0x13: 7}
    if t in fixed:
        n = fixed[t]
    elif t == 0x0E:
        if p + 1 >= end:
            return 0
        n = 2 + b[p + 1]
    else:
        return 0
    return n if p + n <= end else 0


def main():
    scm, inc, cnt, enum, out = sys.argv[1:6]
    b = open(scm, 'rb').read()
    known = {}
    for m in re.finditer(r'\{0x([0-9A-Fa-f]{4}),\s*(\d+),\s*(\d+),\s*(\d+)\}', open(inc).read()):
        known[int(m.group(1), 16)] = int(m.group(2))
    names = {int(m.group(2), 16): m.group(1) for m in re.finditer(r'COMMAND_(\w+)\s*=\s*0x([0-9A-Fa-f]+)', open(enum).read())}
    lower = {}
    for k, v in json.load(open(cnt)).items():
        lower[int(k)] = sum(max(x) if x else 0 for x in v[:2])
    # mission offsets
    off = 0
    offs = []
    for _ in range(6):
        if not (b[off] == 2 and b[off + 1] == 0 and b[off + 2] == 1):
            break
        nxt = struct.unpack_from('<I', b, off + 3)[0]
        idx = b[off + 7]
        if idx == 1:
            n = struct.unpack_from('<H', b, off + 16)[0]
            offs = [struct.unpack_from('<I', b, off + 24 + 4 * k)[0] for k in range(n)]
        off = nxt
    ends = offs[1:] + [len(b)]
    assigned = {}
    for mi, (s, e) in enumerate(zip(offs, ends)):
        snap = dict(assigned)
        res = solve(b, s, e, known, names, lower, assigned)
        if not res:
            assigned.clear()
            assigned.update(snap)
        print('mission %d %06X..%06X %s (%d new ops)' % (mi, s, e, 'ok' if res else 'FAILED', len(assigned)), file=sys.stderr)
    with open(out, 'w') as f:
        for op in sorted(assigned):
            f.write('{0x%04X, %d, 0, 0},\n' % (op, assigned[op]))


JUMPS = {0x0002, 0x004C, 0x004D, 0x0050, 0x004F}   # argument 0 is a code address (negative: from the block start)


def parse_cmd(b, pos, end, op, n, variadic):
    """End of the command whose opcode sits at pos with n arguments, or -1; also its first argument."""
    p = pos + 2
    first = None
    if variadic:
        k = 0
        while p < end and b[p] != 0:
            l = arg_len(b, p, end)
            if not l:
                return -1, None
            if k == 0 and b[p] == 1:
                first = struct.unpack_from('<i', b, p + 1)[0]
            p += l
            k += 1
        return (p + 1, first) if p < end else (-1, None)
    for k in range(n):
        l = arg_len(b, p, end)
        if not l:
            return -1, None
        if k == 0 and b[p] == 1:
            first = struct.unpack_from('<i', b, p + 1)[0]
        p += l
    return p, first


def solve(b, start, end, known, names, lower, assigned):
    stack = []          # choice points: [pos, op, candidates, index, lens]
    starts, startset, pend = [], set(), []
    pos = start
    steps = 0
    far = start

    def enter(pos):
        """Register a command start; False when a pending jump target was skipped."""
        starts.append(pos)
        startset.add(pos)
        for t in pend:
            if t < pos and t not in startset:
                return False
        return True

    def leave(lens):
        while len(starts) > lens[0]:
            startset.discard(starts.pop())
        del pend[lens[1]:]

    def jump(op, first):
        if op in JUMPS and first is not None and first < 0:
            t = start - first
            if t < start or t >= end:
                return True
            if t < pos_now[0]:
                return t in startset
            pend.append(t)
        return True

    pos_now = [pos]
    while True:
        far = max(far, pos)
        if pos == end:
            if all(t in startset for t in pend):
                return True
            ok = False
        else:
            ok = False
            if steps >= 5_000_000:
                print('  budget: farthest %06X' % far, file=sys.stderr)
                return False
            steps += 1
            pos_now[0] = pos
            lens = (len(starts), len(pend))
            op = (b[pos] | (b[pos + 1] << 8)) & 0x7FFF if pos + 2 <= end else -1
            if op in names or op in known:
                if not enter(pos):
                    leave(lens)
                else:
                    if op in VARIADIC:
                        np, first = parse_cmd(b, pos, end, 0, 0, True)
                        if np >= 0 and jump(op, first):
                            pos = np
                            ok = True
                    elif op in known or op in assigned:
                        np, first = parse_cmd(b, pos, end, op, known.get(op, assigned.get(op)), False)
                        if np >= 0 and jump(op, first):
                            pos = np
                            ok = True
                    else:
                        lo = lower.get(op, 0)
                        cands = list(range(lo, 21)) + list(range(0, lo))
                        stack.append([pos, op, cands, 0, lens])
                        if try_choice(b, stack, assigned, end, jump, pos_now):
                            pos = stack[-1][5]
                            ok = True
                        else:
                            stack.pop()
                            leave(lens)
        if ok:
            continue
        # backtrack: advance the last choice point
        while True:
            if not stack:
                print('  stuck: farthest %06X steps %d' % (far, steps), file=sys.stderr)
                return False
            ch = stack[-1]
            leave(ch[4])
            assigned.pop(ch[1], None)
            ch[3] += 1
            enter(ch[0])
            pos_now[0] = ch[0]
            if try_choice(b, stack, assigned, end, jump, pos_now):
                pos = ch[5]
                break
            leave(ch[4])
            stack.pop()
        continue


def try_choice(b, stack, assigned, end, jump, pos_now):
    ch = stack[-1]
    pos, op, cands = ch[0], ch[1], ch[2]
    i = ch[3]
    while i < len(cands):
        np, first = parse_cmd(b, pos, end, op, cands[i], False)
        if np >= 0:
            ch[3] = i
            if jump(op, first):
                if len(ch) == 5:
                    ch.append(np)
                else:
                    ch[5] = np
                assigned[op] = cands[i]
                return True
        i += 1
    return False


main()
