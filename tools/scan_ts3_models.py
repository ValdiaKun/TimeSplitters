#!/usr/bin/env python3
"""Heuristically identify TS3/Future Perfect model records in P5CK payloads."""
from __future__ import annotations
import argparse, math, struct
from pathlib import Path

def entries(p):
    with p.open("rb") as f:
        if f.read(4) != b"P5CK": raise ValueError("not P5CK")
        off,size=struct.unpack("<II",f.read(8)); f.seek(off)
        return [struct.unpack("<IIII",f.read(16)) for _ in range(size//16)]

def score(data):
    if len(data)<32: return 0,None
    mo,io,uo=struct.unpack_from("<III",data,0)
    if not (0 < mo < len(data) and 0 < io < len(data) and (uo==0 or uo < len(data))): return 0,None
    if mo%4 or io%4: return 0,None
    # TS3Model ModelInfo: int numSubMeshes, 8 bytes, float scale, int, 3 floats, 2 ints
    if io+36>len(data): return 0,None
    n=struct.unpack_from("<i",data,io)[0]
    scale=struct.unpack_from("<f",data,io+12)[0]
    s=0
    if 1<=n<=128: s+=3
    if math.isfinite(scale) and 0.0001<abs(scale)<10000: s+=3
    if mo>=12 and io>=12: s+=1
    # material block should begin with plausible count/offset structure
    mcount=struct.unpack_from("<i",data,mo)[0] if mo+4<=len(data) else -1
    if 0<=mcount<=256: s+=2
    return s,(mo,io,uo,n,scale,mcount)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("pak",type=Path)
    ap.add_argument("--top",type=int,default=40)
    ap.add_argument("--sample",type=int,default=4*1024*1024)
    args=ap.parse_args()
    es=entries(args.pak); hits=[]
    with args.pak.open("rb") as f:
        for i,(crc,off,length,stored) in enumerate(es):
            n=min(length,args.sample); f.seek(off); data=f.read(n)
            s,info=score(data)
            if s>=7: hits.append((s,i,crc,off,length,info))
    hits.sort(reverse=True)
    print(f"PAK: {args.pak}")
    print(f"Model candidates: {len(hits)}")
    for s,i,crc,off,length,info in hits[:args.top]:
        mo,io,uo,n,scale,mcount=info
        print(f"{i:04d} score={s} crc={crc:08x} off=0x{off:x} len={length} matOff=0x{mo:x} infoOff=0x{io:x} unkOff=0x{uo:x} submeshes={n} scale={scale:g} matCount={mcount}")

if __name__=="__main__": main()
