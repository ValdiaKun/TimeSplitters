#!/usr/bin/env python3
"""Extract locally the Future Perfect resource records most useful for RE."""
from __future__ import annotations
import argparse, gzip, struct
from pathlib import Path

MAGIC = bytes.fromhex("c4 6e 8e 40")

def entries(p):
    with p.open("rb") as f:
        if f.read(4) != b"P5CK": raise ValueError("not P5CK")
        io, il = struct.unpack("<II", f.read(8))
        f.seek(io)
        return [struct.unpack("<IIII", f.read(16)) for _ in range(il//16)]

def strings(data):
    out=[]; cur=bytearray()
    for b in data:
        if 32 <= b < 127: cur.append(b)
        else:
            if len(cur)>=4: out.append(cur.decode("ascii","replace"))
            cur.clear()
    if len(cur)>=4: out.append(cur.decode("ascii","replace"))
    return out

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("pak",type=Path)
    ap.add_argument("--out",type=Path,required=True)
    ap.add_argument("--max-total",type=int,default=64*1024*1024)
    ap.add_argument("--inflate-gzip",action="store_true")
    a=ap.parse_args()
    rows=entries(a.pak); a.out.mkdir(parents=True,exist_ok=True)
    total=0; n=0
    with a.pak.open("rb") as f:
        for i,(crc,off,length,stored) in enumerate(rows):
            f.seek(off); head=f.read(min(length,64))
            if not head.startswith(MAGIC): continue
            size=stored or length
            if total+size>a.max_total: break
            f.seek(off); data=f.read(size)
            inflated=False
            if a.inflate_gzip and data.startswith(b"\x1f\x8b"):
                data=gzip.decompress(data); inflated=True
            ss=strings(data[:65536])
            label=next((s for s in ss if "/" in s or "_" in s), f"entry_{i:04d}")
            safe="".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in label)[:80]
            suffix=".inflated.bin" if inflated else (".gz" if stored else ".bin")
            out=a.out/f"{i:04d}_{crc:08x}_{safe}{suffix}"
            out.write_bytes(data)
            total+=len(data); n+=1
    print(f"Extracted {n} FP resource records ({total} bytes) to {a.out}")

if __name__=="__main__": main()
