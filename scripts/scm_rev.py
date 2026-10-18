#!/usr/bin/env python3
"""Typed script command signatures from a reversed-engineering source tree.

usage: scm_rev.py <reversed source root> <out rev.json>
writes rev.json (exact argument counts), auto.json (return size unknown) and revmeta.json
(opcode -> [inputs, outputs or -1, returns a condition]).
"""
import re,os,json,sys
R=sys.argv[1].rstrip('/')+'/source/game_sa/'
enum={}
for m in re.finditer(r'(COMMAND_\w+)\s*=\s*(0x[0-9A-Fa-f]+)',open(R+'Enums/eScriptCommands.h').read()):
    enum[m.group(1)]=int(m.group(2),16)
SZ={'CVector':3,'CVector2D':2,'CRect':4,'CRGBA':4,'CBox':6,'CSphere':4,'CColSphere':4}
def split(s):
    out=[];d=0;cur=''
    for c in s:
        if c in '<(': d+=1
        if c in '>)': d-=1
        if c==',' and d==0: out.append(cur);cur=''
        else: cur+=c
    if cur.strip(): out.append(cur)
    return out
def tsize(t):
    t=re.sub(r'\b(const|struct|class)\b','',t).replace('&','').replace('*','').strip()
    t=re.sub(r'\s+',' ',t)
    base=t.split('<')[0].strip()
    return SZ.get(base,1)
def pcount(params):
    n=0;var=False
    for p in split(params):
        p=p.strip()
        if not p: continue
        if 'CRunningScript' in p: continue
        if 'Multi' in p or '...' in p: var=True
        # drop name
        t=re.sub(r'\s*\b\w+\s*(=.*)?$','',p) if re.search(r'\w\s+\**&?\w+\s*(=.*)?$',p) else p
        n+=tsize(t)
    return n,var
def rsize(r):
    r=r.strip()
    if r in('void','bool','OpcodeResult','notsa::script::OpcodeResult'): return [0]
    if r=='auto': return [0,1,3,2,4,5,6]
    m=re.match(r'(?:notsa::script::)?MultiRet<(.*)>',r)
    if m: return [sum(tsize(x) for x in split(m.group(1)))]
    if r.startswith('std::tuple'): return [sum(tsize(x) for x in split(r[r.index('<')+1:r.rindex('>')]))]
    return [tsize(r)]
res={}
auto={}
meta={}
cdir=R+'Scripts/Commands/'
for f in os.listdir(cdir):
    if not f.endswith('.cpp'): continue
    s=open(cdir+f,errors='ignore').read()
    funcs={}
    for m in re.finditer(r'^((?:template<[^>]*>\s*)?)([\w:<>, *&]+?)\s+(\w+)\(([^)]*)\)\s*(?:->[^{]*)?\{',s,re.M):
        funcs[m.group(3)]=(m.group(2),m.group(4))
    for m in re.finditer(r'^\s*REGISTER_COMMAND_HANDLER\((COMMAND_\w+),\s*(\w+)',s,re.M):
        c,fn=m.groups()
        if c not in enum or fn not in funcs: continue
        r,p=funcs[fn]
        n,var=pcount(p)
        if var: continue
        meta[enum[c]]=[n, -1 if r.strip()=='auto' else rsize(r)[0], 1 if r.strip()=='bool' else 0]
        if r.strip()=='auto': auto[enum[c]]=[n+x for x in rsize(r)]
        else: res[enum[c]]=sorted({n+x for x in rsize(r)})
json.dump({str(k):v for k,v in res.items()},open(sys.argv[2],'w'))
json.dump({str(k):v for k,v in auto.items()},open(sys.argv[2].replace('rev','auto'),'w'))
json.dump({str(k):v for k,v in meta.items()},open(sys.argv[2].replace('rev','revmeta'),'w'))
print(len(res),'opcodes',sum(len(v)>1 for v in res.values()),'ambiguous')
