#!/usr/bin/env python3
"""Mine CollectParameters / StoreParameters counts from decompiled command handlers.

usage: scm_mine.py <decomp root> <out cnt.json>   (map opcode -> [collects, stores, size])
"""
import re,glob,sys,json
D=sys.argv[1].rstrip('/')+'/src/CRunningScript/'
res={}
for f in sorted(glob.glob(D+'ProcessCommands*.c')):
    labels=[];body=[];groups=[]
    for line in open(f,errors='replace'):
        m=re.match(r'\s{2}case (0x[0-9a-fA-F]+|\d+):\s*$',line)
        if m:
            if body:
                groups.append((labels,''.join(body))); labels=[];body=[]
            labels.append(int(m.group(1),0)); continue
        if re.match(r'\s{2}default:',line):
            if body: groups.append((labels,''.join(body))); labels=[];body=[]
            labels=[];continue
        if labels: body.append(line)
    if labels and body: groups.append((labels,''.join(body)))
    base=int(re.search(r'ProcessCommands(\d+)To',f).group(1))
    allv=[l for g in groups for l in g[0]]
    off = base if allv and min(allv) < base else 0
    for labels,b in groups:
        c=[int(x,0) for x in re.findall(r'CollectParameters\(\w+,\s*(0x[0-9a-fA-F]+|\d+)\)',b)]
        s=[int(x,0) for x in re.findall(r'StoreParameters\(\w+,\s*(0x[0-9a-fA-F]+|\d+)\)',b)]
        for l in labels: res[l+off]=(c,s,len(b))
json.dump({str(k):v for k,v in res.items()},open(sys.argv[2],'w'))
print(len(res))
