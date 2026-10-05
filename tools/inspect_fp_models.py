#!/usr/bin/env python3
"""Find and summarize observed Future Perfect PS2 model records."""
from __future__ import annotations
import argparse, json, struct
from pathlib import Path

def entries(p):
    with p.open("rb") as f:
        if f.read(4) != b"P5CK":
            return []
        index_offset, index_length = struct.unpack("<II", f.read(8))
        if index_length % 16:
            raise ValueError("invalid P5CK index length")
        f.seek(index_offset)
        return [struct.unpack("<IIII", f.read(16)) for _ in range(index_length // 16)]

def probe(data):
    if len(data) < 16:
        return None
    material_offset, info_offset, auxiliary_offset, _ = struct.unpack("<4I", data[:16])
    if min(material_offset, info_offset, auxiliary_offset) < 0x40:
        return None
    if max(material_offset, info_offset, auxiliary_offset) >= len(data):
        return None
    if (material_offset | info_offset | auxiliary_offset) & 3:
        return None
    if len({material_offset, info_offset, auxiliary_offset}) != 3:
        return None
    if info_offset + 40 > len(data):
        return None
    mesh_count = struct.unpack_from("<I", data, info_offset)[0]
    scale = struct.unpack_from("<f", data, info_offset + 36)[0]
    if not (0 < mesh_count <= 4096):
        return None
    if not (scale == scale) or abs(scale) < 1e-12 or abs(scale) > 1000:
        return None
    return {"material_offset": material_offset, "info_offset": info_offset,
            "auxiliary_offset": auxiliary_offset, "mesh_count": mesh_count,
            "scale": scale}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root", type=Path)
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()
    rows = []
    for pak in sorted(args.root.rglob("*.PAK")):
        es = entries(pak)
        if not es:
            continue
        with pak.open("rb") as f:
            for i, (crc, off, length, stored) in enumerate(es):
                size = stored or length
                if size < 16:
                    continue
                f.seek(off)
                h = probe(f.read(size))
                if h:
                    rows.append({"pak": str(pak), "entry": i,
                                 "crc": f"{crc:08x}", "length": length,
                                 "stored": stored, **h})
    print(f"Observed model candidates: {len(rows)}")
    for row in rows[:40]:
        print(f"{row['pak']} entry={row['entry']} crc={row['crc']} "
              f"meshes={row['mesh_count']} scale={row['scale']:.5g}")
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(rows, indent=2), encoding="utf-8")
        print("Report:", args.out)

if __name__ == "__main__":
    main()
