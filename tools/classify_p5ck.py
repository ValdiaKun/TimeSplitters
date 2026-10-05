#!/usr/bin/env python3
"""Classify Future Perfect P5CK entries using lightweight signatures and strings."""
from __future__ import annotations
import argparse
import collections
import re
import struct
from pathlib import Path

def read_entries(path):
    with path.open("rb") as f:
        if f.read(4) != b"P5CK":
            raise ValueError(f"{path}: not P5CK")
        index_offset, index_length = struct.unpack("<II", f.read(8))
        if index_length % 16:
            raise ValueError("invalid P5CK index")
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

def classify(data, strings_found):
    if data.startswith(b"c4\x6e\x8e\x40"):
        return "FP_RESOURCE"
    if data.startswith(b"\x89PNG"):
        return "PNG"
    if data.startswith(b"DDS "):
        return "DDS"
    if data.startswith(b"RIFF"):
        return "RIFF"
    if data.startswith(b"OggS"):
        return "OGG"
    if data.startswith(b"\xff\xd8\xff"):
        return "JPEG"
    if data.startswith(b"\x1f\x8b"):
        return "GZIP"
    joined = " ".join(strings_found).lower()
    if "texture" in joined or "material" in joined:
        return "TEXTURE_OR_MATERIAL"
    if "anim" in joined:
        return "ANIMATION"
    if "sound" in joined or "music" in joined:
        return "AUDIO"
    return "BINARY"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pak", type=Path)
    ap.add_argument("--sample", type=int, default=65536)
    ap.add_argument("--strings", type=int, default=6)
    ap.add_argument("--report", type=Path)
    args = ap.parse_args()

    entries = read_entries(args.pak)
    rows = []
    with args.pak.open("rb") as f:
        for i, (crc, offset, length, stored) in enumerate(entries):
            n = min(stored or length, args.sample)
            f.seek(offset)
            data = f.read(n)
            ss = strings(data)
            rows.append((i, crc, offset, length, stored, classify(data, ss), data[:16].hex(" "), ss[:args.strings]))

    counts = collections.Counter(r[5] for r in rows)
    print(f"PAK: {args.pak}")
    print(f"Entries: {len(rows)}")
    print("Classes:")
    for k, v in counts.most_common():
        print(f"  {k}: {v}")
    print("\nCandidates:")
    for i, crc, offset, length, stored, cls, head, ss in rows:
        if cls != "BINARY" or any(s for s in ss):
            label = " | ".join(ss)
            print(f"{i:04d} {cls:22s} crc={crc:08x} off=0x{offset:x} len={length} head={head} {label}")

    if args.report:
        with args.report.open("w", encoding="utf-8") as out:
            out.write("entry\tcrc\toffset\tlength\tstored\tclass\thead\tstrings\n")
            for r in rows:
                out.write("\t".join([str(r[0]), f"{r[1]:08x}", f"0x{r[2]:x}", str(r[3]), str(r[4]), r[5], r[6], " | ".join(r[7])]) + "\n")
        print(f"\nReport: {args.report}")

if __name__ == "__main__":
    main()
