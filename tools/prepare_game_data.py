#!/usr/bin/env python3
"""Prepare locally owned TimeSplitters: Future Perfect data for development."""
from __future__ import annotations
import argparse, hashlib, shutil
from pathlib import Path
import pycdlib

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "game_data"

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def extract_iso(iso_path: Path, out_dir: Path) -> int:
    if not iso_path.is_file():
        print(f"ISO not found: {iso_path}")
        return 2
    print(f"ISO: {iso_path}")
    print(f"SHA-256: {sha256(iso_path)}")
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)
    cd = pycdlib.PyCdlib()
    try:
        cd.open(str(iso_path))
        entries = []
        for dirname, _, filelist in cd.walk(iso_path="/"):
            for filename in filelist:
                full = (dirname.rstrip("/") + "/" + filename).replace("//", "/")
                entries.append(full)
        for full in entries:
            try:
                name = full.rsplit("/", 1)[-1]
                target = out_dir / name
                with target.open("wb") as fp:
                    cd.get_file_from_iso_fp(fp, iso_path=full)
            except Exception:
                pass
    finally:
        cd.close()

    candidates = sorted(
        p for p in out_dir.rglob("*")
        if p.is_file() and p.suffix.lower() in {".p5ck", ".pak"}
    )
    print(f"Extracted local game data to: {out_dir}")
    if candidates:
        print("Candidate archives:")
        for p in candidates:
            print(f"  - {p.relative_to(out_dir)}")
    else:
        print("No .p5ck/.pak files detected; inspect game_data manually.")
    print("Nothing under game_data should be committed to Git.")
    return 0

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("iso", type=Path, help="Path to your legally obtained game ISO")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()
    return extract_iso(args.iso.expanduser().resolve(), args.out.resolve())

if __name__ == "__main__":
    raise SystemExit(main())
