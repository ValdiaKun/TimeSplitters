#!/usr/bin/env python3
"""Inspect selected Future Perfect P5CK payloads without extracting the whole archive."""
from __future__ import annotations
import argparse
import struct
from pathlib import Path

def read_entries(path: Path):
    with path.open("rb") as f:
        if f.read(4) != b"P5CK":
            raise ValueError(f"{path}: not P5CK")
        index_offset, index_length = struct.unpack("<II", f.read(8))
        if index_length % 16:
            raise ValueError(f"{path}: invalid index length")
        f.seek(index_offset)
        for i in range(index_length // 16):
            crc, offset, length, stored = struct.unpack("<IIII", f.read(16))
            yield i, crc, offset, length, stored

def ascii_strings(data: bytes, minimum=4):
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

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pak", type=Path)
    ap.add_argument("--count", type=int, default=12)
    ap.add_argument("--max-bytes", type=int, default=262144)
    args = ap.parse_args()
    rows = list(read_entries(args.pak))
    print(f"P5CK: {args.pak}")
    print(f"Entries: {len(rows)}")
    with args.pak.open("rb") as f:
        for i, crc, offset, length, stored in rows[:args.count]:
            f.seek(offset)
            data = f.read(min(stored or length, args.max_bytes))
            magic = data[:16].hex(" ")
            strings = ascii_strings(data)
            print(f"ENTRY {i:04d} CRC={crc:08x} OFFSET=0x{offset:x} LENGTH={length} STORED={stored}")
            print(f"  HEAD {magic}")
            if strings:
                print("  STRINGS", " | ".join(strings[:12]))
            else:
                print("  STRINGS <none>")

if __name__ == "__main__":
    main()
