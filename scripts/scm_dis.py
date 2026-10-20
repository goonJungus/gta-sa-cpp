#!/usr/bin/env python3
"""Disassemble a range of main.scm: scm_dis.py <main.scm> <start hex> [count] [base hex]"""
import re, struct, sys
root = __file__.rsplit('/', 2)[0] + '/host/engine/'
args = {int(m.group(1), 16): int(m.group(2)) for m in re.finditer(r'\{(0x[0-9A-Fa-f]+), (\d+),', open(root + 'ScmArgs.inc').read())}
names = {int(m.group(1), 16): m.group(2) for m in re.finditer(r'\{(0x[0-9A-Fa-f]+), "(\w+)"\}', open(root + 'ScmNames.inc').read())}
d = open(sys.argv[1], 'rb').read()
pc = int(sys.argv[2], 16)
count = int(sys.argv[3]) if len(sys.argv) > 3 else 60
def arg(p):
    t = d[p]; p += 1
    if t == 1: return p + 4, str(struct.unpack_from('<i', d, p)[0])
    if t == 2: return p + 2, 'g%d' % struct.unpack_from('<H', d, p)[0]
    if t == 3: return p + 2, 'l%d' % struct.unpack_from('<H', d, p)[0]
    if t == 4: return p + 1, str(struct.unpack_from('<b', d, p)[0])
    if t == 5: return p + 2, str(struct.unpack_from('<h', d, p)[0])
    if t == 6: return p + 4, '%g' % struct.unpack_from('<f', d, p)[0]
    if t in (7, 8, 0xC, 0xD, 0x12, 0x13): return p + 6, 'arr@%d[%d]' % struct.unpack_from('<HH', d, p)
    if t == 9: return p + 8, repr(d[p:p+8].split(b'\0')[0].decode('latin1'))
    if t == 0xF: return p + 16, repr(d[p:p+16].split(b'\0')[0].decode('latin1'))
    if t == 0xE: n = d[p]; return p + 1 + n, repr(d[p+1:p+1+n].decode('latin1'))
    if t in (0xA, 0xB, 0x10, 0x11): return p + 2, 'str%d' % struct.unpack_from('<H', d, p)[0]
    raise ValueError('arg type %d at %X' % (t, p))
for _ in range(count):
    op = struct.unpack_from('<H', d, pc)[0]
    start = pc; pc += 2
    neg = 'NOT ' if op & 0x8000 else ''
    op &= 0x7FFF
    out = []
    try:
        if op == 0x004F or op == 0x0913:
            p, a = arg(pc); out.append(a); pc = p
            while d[pc] != 0: p, a = arg(pc); out.append(a); pc = p
            pc += 1
        else:
            n = args.get(op, 2 if 4 <= op <= 0x93 else 0)
            if op == 0x03A4: n = 1
            for _ in range(n): p, a = arg(pc); out.append(a); pc = p
    except Exception as e:
        out.append('<%s>' % e); print('%06X %s%04X %s' % (start, neg, op, ' '.join(out))); break
    print('%06X %s%s %s' % (start, neg, names.get(op, '%04X' % op), ' '.join(out)))
