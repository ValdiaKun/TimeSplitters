#!/usr/bin/env python3
"""Extract a locally owned Future Perfect ISO and inspect P5CK archives."""
from __future__ import annotations
import argparse
import hashlib
import shutil
import struct
from pathlib import Path

SECTOR = 2048

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def dir_records(f, extent: int, size: int):
    f.seek(extent * SECTOR)
    data = f.read(size)
    i = 0
    while i < len(data):
        length = data[i]
        if length == 0:
            i = ((i // SECTOR) + 1) * SECTOR
            continue
        record = data[i:i + length]
        if len(record) < 34:
            break
        entry_extent = struct.unpack_from("<I", record, 2)[0]
        entry_size = struct.unpack_from("<I", record, 10)[0]
        flags = record[25]
        name_len = record[32]
        raw_name = record[33:33 + name_len]
        if raw_name not in (b"\x00", b"\x01"):
            name = raw_name.decode("ascii", "replace").rstrip(";1")
            yield name, entry_extent, entry_size, flags
        i += length

def extract_iso(iso: Path, out: Path):
    with iso.open("rb") as f:
        f.seek(16 * SECTOR)
        vd = f.read(SECTOR)
        if vd[:7] != b"\x01CD001\x01":
            raise ValueError("No ISO9660 Primary Volume Descriptor found")
        root = vd[156:190]
        extent = struct.unpack_from("<I", root, 2)[0]
        size = struct.unpack_from("<I", root, 10)[0]

        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True)
        files = []

        def walk(dir_extent: int, dir_size: int, rel: Path):
            for name, child_extent, child_size, flags in dir_records(f, dir_extent, dir_size):
                target = out / rel / name
                if flags & 2:
                    target.mkdir(parents=True, exist_ok=True)
                    walk(child_extent, child_size, rel / name)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    f.seek(child_extent * SECTOR)
                    remaining = child_size
                    with target.open("wb") as dst:
                        while remaining:
                            chunk = f.read(min(remaining, 1024 * 1024))
                            if not chunk:
                                raise IOError("Unexpected end of ISO")
                            dst.write(chunk)
                            remaining -= len(chunk)
                    files.append(target)
        walk(extent, size, Path())
    return files

def inspect_p5ck(path: Path):
    with path.open("rb") as f:
        header = f.read(12)
        if header[:4] != b"P5CK":
            return None
        index_offset, index_length = struct.unpack("<II", header[4:12])
        if index_length % 16:
            raise ValueError(f"{path}: invalid P5CK index length")
        count = index_length // 16
        entries = []
        f.seek(index_offset)
        for _ in range(count):
            crc, offset, length, gz_length = struct.unpack("<IIII", f.read(16))
            entries.append((crc, offset, length, gz_length))
        return index_offset, entries

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("iso", type=Path)
    parser.add_argument("--out", type=Path, default=Path("game_data"))
    parser.add_argument("--extract-p5ck", action="store_true")
    args = parser.parse_args()
    iso = args.iso.expanduser().resolve()
    out = args.out.expanduser().resolve()
    print("ISO:", iso)
    print("SHA-256:", sha256(iso))
    print("Size:", iso.stat().st_size)
    files = extract_iso(iso, out)
    print(f"Extracted {len(files)} files to {out}")
    paks = [p for p in files if p.suffix.upper() == ".PAK"]
    print(f"Found {len(paks)} PAK files")
    for pak in paks:
        info = inspect_p5ck(pak)
        if not info:
            print("Non-P5CK PAK:", pak.relative_to(out))
            continue
        index_offset, entries = info
        print(f"P5CK {pak.relative_to(out)}: {len(entries)} entries, index @ 0x{index_offset:X}")
        if args.extract_p5ck:
            target_dir = out / "p5ck" / pak.relative_to(out).with_suffix("")
            target_dir.mkdir(parents=True, exist_ok=True)
            with pak.open("rb") as f:
                for i, (crc, offset, length, gz_length) in enumerate(entries):
                    stored_length = gz_length if gz_length else length
                    suffix = ".gz" if gz_length else ".bin"
                    f.seek(offset)
                    (target_dir / f"{i:05d}_{crc:08x}{suffix}").write_bytes(f.read(stored_length))

if __name__ == "__main__":
    main()
