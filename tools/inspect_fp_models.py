#!/usr/bin/env python3
"""Find Future Perfect PS2 model-like PAK entries."""
from __future__ import annotations
import argparse,json,struct
from pathlib import Path

def entries(p):
    with p.open('rb') as f:
        if f.read(4)!=b'P5CK': return []
        io,il=struct.unpack('<II',f.read(8)); f.seek(io)
        return [struct.unpack('<IIII',f.read(16)) for _ in range(il//16)]

def probe(data):
    if len(data)<16: return None
    a,b,c,flags=struct.unpack('<IIII',data[:16])
    if flags!=0 or min(a,b,c)<0x40 or max(a,b,c)>=len(data) or (a|b|c)&3: return None
    if len({a,b,c})<3: return None
    return {'chunk_a':a,'chunk_b':b,'chunk_c':c,'flags':flags}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('root',type=Path); ap.add_argument('--out',type=Path); a=ap.parse_args()
    rows=[]
    for pak in sorted(a.root.rglob('*.PAK')):
        es=entries(pak)
        with pak.open('rb') as f:
            for i,(crc,off,length,stored) in enumerate(es):
                size=stored or length
                if size<16: continue
                f.seek(off); h=probe(f.read(16))
                if h: rows.append({'pak':str(pak),'entry':i,'crc':f'{crc:08x}','length':length,'stored':stored,**h})
    print(f'Model-like entries: {len(rows)}')
    counts={}
    for r in rows: counts[r['pak']]=counts.get(r['pak'],0)+1
    for k,v in sorted(counts.items(),key=lambda x:(-x[1],x[0])): print(v,k)
    if a.out:
        a.out.parent.mkdir(parents=True,exist_ok=True); a.out.write_text(json.dumps(rows,indent=2),encoding='utf-8'); print('Report:',a.out)

if __name__=='__main__': main()
