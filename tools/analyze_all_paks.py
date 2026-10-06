#!/usr/bin/env python3
"""Build a compact reverse-engineering report for all P5CK archives in game_data."""
from __future__ import annotations
import argparse, collections, json, re, struct
from pathlib import Path

FP_MAGIC = bytes.fromhex("c4 6e 8e 40")

def read_entries(path):
    with path.open("rb") as f:
        if f.read(4) != b"P5CK":
            return []
        index_offset, index_length = struct.unpack("<II", f.read(8))
        if index_length % 16:
            return []
        f.seek(index_offset)
        return [struct.unpack("<IIII", f.read(16)) for _ in range(index_length // 16)]

def strings(data, minimum=4):
    out, cur = [], bytearray()
    for b in data:
        if 32 <= b < 127:
            cur.append(b)
        else:
            if len(cur) >= minimum:
                out.append(cur.decode("ascii", "replace"))
            cur.clear()
    if len(cur) >= minimum:
        out.append(cur.decode("ascii", "replace"))
    return out

def classify(data, ss):
    if data.startswith(FP_MAGIC): return "FP_RESOURCE"
    if data.startswith(b"PNG\r\n\x1a\n"): return "PNG"
    if data.startswith(b"DDS "): return "DDS"
    if data.startswith(b"RIFF"): return "RIFF"
    if data.startswith(b"OggS"): return "OGG"
    if data.startswith(b"\xff\xd8\xff"): return "JPEG"
    if data.startswith(b"\x1f\x8b"): return "GZIP"
    s = " ".join(ss).lower()
    if "headersound" in s or "numsoundpads" in s: return "AUDIO_META"
    if "texture" in s or "material" in s: return "TEXTURE_OR_MATERIAL"
    if "anim" in s: return "ANIMATION"
    if "mesh" in s or "vertex" in s or "polygon" in s: return "GEOMETRY_HINT"
    return "BINARY"

def scan(pak, sample):
    entries = read_entries(pak)
    out=[]
    with pak.open("rb") as f:
        for i,(crc,off,length,stored) in enumerate(entries):
            stored_length=stored or length
            n=min(stored_length,sample)
            f.seek(off); data=f.read(n)
            ss=strings(data)
            is_gzip=data.startswith(b"\x1f\x8b")
            out.append({
                "entry":i,"crc":f"{crc:08x}","offset":off,"length":length,"stored":stored,
                "stored_length":stored_length,"compressed":bool(stored),"gzip":is_gzip,
                "class":classify(data,ss),"head":data[:16].hex(" "),"strings":ss[:20]
            })
    return out

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("root",type=Path)
    ap.add_argument("--sample",type=int,default=65536)
    ap.add_argument("--out",type=Path,default=None)
    a=ap.parse_args()
    paks=sorted(a.root.rglob("*.PAK"))+sorted(a.root.rglob("*.pak"))
    # de-duplicate paths
    paks=list(dict.fromkeys(paks))
    report={"archives":[],"totals":collections.Counter(),"fp_candidates":[]}
    for pak in paks:
        rows=scan(pak,a.sample)
        counts=collections.Counter(r["class"] for r in rows)
        item={"pak":str(pak),"entries":len(rows),"classes":dict(counts),"rows":rows}
        report["archives"].append(item)
        report["totals"].update(counts)
        for r in rows:
            if r["class"] in ("FP_RESOURCE","GEOMETRY_HINT","TEXTURE_OR_MATERIAL","AUDIO_META"):
                report["fp_candidates"].append({"pak":str(pak),**r})
        print(f"{pak}: {len(rows)} entries; " + ", ".join(f"{k}={v}" for k,v in counts.most_common()))
    report["totals"]=dict(report["totals"])
    if a.out:
        a.out.parent.mkdir(parents=True,exist_ok=True)
        a.out.write_text(json.dumps(report,indent=2),encoding="utf-8")
        print(f"Report: {a.out}")
    print(f"Archives: {len(paks)}")
    print(f"Candidates: {len(report['fp_candidates'])}")

if __name__=="__main__":
    main()
