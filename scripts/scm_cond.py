#!/usr/bin/env python3
"""Find which opcodes are used as conditions in a compiled script.

usage: scm_cond.py <main.scm> <out ScmCond.inc>
Every ANDOR n announces (n % 20) + 1 condition instructions; each opcode seen in such a slot is a condition.
"""
import re, struct, sys
root = __file__.rsplit('/', 2)[0] + '/host/engine/'
args = {int(m.group(1), 16): int(m.group(2)) for m in re.finditer(r'\{(0x[0-9A-Fa-f]+), (\d+),', open(root + 'ScmArgs.inc').read())}
d = open(sys.argv[1], 'rb').read()

def arg(p):
    t = d[p]; p += 1
    sizes = {1: 4, 2: 2, 3: 2, 4: 1, 5: 2, 6: 4, 7: 6, 8: 6, 0xC: 6, 0xD: 6, 0x12: 6, 0x13: 6, 9: 8, 0xF: 16, 0xA: 2, 0xB: 2, 0x10: 2, 0x11: 2}
    if t == 0xE: return p + 1 + d[p], t, None
    if t not in sizes: raise ValueError
    v = struct.unpack_from('<i', d, p)[0] if t == 1 else struct.unpack_from('<b', d, p)[0] if t == 4 else struct.unpack_from('<h', d, p)[0] if t == 5 else None
    return p + sizes[t], t, v

def step(pc):
    op = struct.unpack_from('<H', d, pc)[0] & 0x7FFF
    pc += 2
    val = None
    if op in (0x004F, 0x0913):
        pc, _, _ = arg(pc)
        while d[pc] != 0: pc, _, _ = arg(pc)
        return op, pc + 1, None
    n = args.get(op, 2 if 4 <= op <= 0x93 else 0)
    if op == 0x03A4: n = 1
    for i in range(n):
        pc, t, v = arg(pc)
        if i == 0: val = v
    return op, pc, val

conds = set()
seen = set()
def scan(pc, limit):
    pending = 0
    last = pc
    try:
        while pc < limit and pc not in seen:
            seen.add(pc)
            last = pc
            op, nxt, val = step(pc)
            if pending:
                conds.add(op); pending -= 1
            if op == 0x00D6 and val is not None:
                pending = val % 20 + 1
            pc = nxt
    except Exception as e:
        print('scan stopped at %X op %04X: %r' % (pc, struct.unpack_from('<H', d, pc)[0], e), file=sys.stderr)
C = 0xCFA4   # chunk 1 of the header: main size, mission count, mission offsets
main_size = struct.unpack_from('<I', d, C + 8)[0]
count = struct.unpack_from('<H', d, C + 16)[0]
offs = struct.unpack_from('<%dI' % count, d, C + 24)
scan(0xDAA8, main_size)
for o in offs:
    scan(o, len(d))
out = sorted(conds)
open(sys.argv[2], 'w').write('// opcodes seen in condition slots of main.scm (scripts/scm_cond.py)\n' + ''.join('0x%04X,\n' % o for o in out))
print(len(out), 'condition opcodes')
