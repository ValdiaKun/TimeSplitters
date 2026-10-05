#!/usr/bin/env python3
"""Validate and resolve a Future Perfect P5CK .c2n checksum/name file.

The tspak implementation reads one line per P5CK entry. The checksum is
parsed from the first token (base-0 integer syntax), while the name begins
at byte 12 of the line. This tool accepts the same practical layout and
also tolerates whitespace-separated variants.
"""
from __future__ import annotations
import argparse
import struct
from pathlib import Path

def p5ck_entries(p: Path):
    with p.open("rb") as f:
        if f.read(4) != b"P5CK":
            raise ValueError(f"{p}: not P5CK")
        index_offset, index_length = struct.unpack("<II", f.read(8))
        if index_length % 16:
            raise ValueError(f"{p}: invalid index length")
        f.seek(index_offset)
        return [struct.unpack("<IIII", f.read(16))
                for _ in range(index_length // 16)]

def parse_line(raw: bytes):
    line = raw.rstrip(b"\r\n")
    if len(line) >= 12:
        try:
            crc = int(line[:10].decode("ascii").strip(), 0)
            name = line[12:].decode("utf-8", "replace").strip()
            if name:
                return crc & 0xffffffff, name
        except ValueError:
            pass
    parts = line.decode("utf-8", "replace").split(None, 1)
    if len(parts) == 2:
        try:
            return int(parts[0], 0) & 0xffffffff, parts[1].strip()
        except ValueError:
            return None
    return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pak", type=Path)
    ap.add_argument("c2n", type=Path)
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()

    entries = p5ck_entries(args.pak)
    lines = args.c2n.read_bytes().splitlines()
    rows = []
    errors = []
    for i, entry in enumerate(entries):
        if i >= len(lines):
            errors.append(f"missing line for entry {i}")
            continue
        parsed = parse_line(lines[i])
        if not parsed:
            errors.append(f"invalid line {i+1}")
            continue
        crc, name = parsed
        expected = entry[0]
        rows.append({"entry": i, "crc": f"{expected:08x}", "name": name,
                     "match": crc == expected})
        if crc != expected:
            errors.append(
                f"entry {i}: PAK CRC {expected:08x} != c2n CRC {crc:08x}"
            )

    if len(lines) != len(entries):
        errors.append(f"line count {len(lines)} != P5CK entry count {len(entries)}")

    if args.out:
        import json
        args.out.write_text(json.dumps(
            {"pak": str(args.pak), "c2n": str(args.c2n),
             "entries": rows, "errors": errors},
            indent=2
        ), encoding="utf-8")

    print(f"PAK entries: {len(entries)}")
    print(f"c2n lines:   {len(lines)}")
    print(f"resolved:    {sum(r['match'] for r in rows)}/{len(rows)}")
    if errors:
        print("Errors:")
        for e in errors[:20]:
            print("  " + e)
        raise SystemExit(1)

if __name__ == "__main__":
    main()
