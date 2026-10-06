#!/usr/bin/env python3
"""Regression tests for local P5CK analysis/extraction helpers."""
from __future__ import annotations
import gzip
import importlib.util
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

def make_pak(path: Path, payload: bytes, compressed: bool):
    stored = gzip.compress(payload, mtime=0) if compressed else payload
    index_offset = 0x10
    data_offset = 0x30
    raw = bytearray(data_offset + len(stored))
    raw[:12] = b"P5CK" + struct.pack("<II", index_offset, 16)
    raw[index_offset:index_offset + 16] = struct.pack("<IIII", 0x12345678,
                                                       data_offset, len(payload),
                                                       len(stored) if compressed else 0)
    raw[data_offset:] = stored
    path.write_bytes(raw)

def main():
    analyzer = load("analyze_all_paks", ROOT / "tools/analyze_all_paks.py")
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        pak = root / "TEST.PAK"
        make_pak(pak, b"c4\x6e\x8e\x40TEST_RESOURCE", True)
        rows = analyzer.scan(pak, 65536)
        assert len(rows) == 1
        assert rows[0]["class"] == "GZIP"
        assert rows[0]["gzip"] is True

        out = root / "out"
        subprocess.run(
            [sys.executable, str(ROOT / "tools/extract_fp_resources.py"),
             str(pak), "--out", str(out), "--inflate-gzip"],
            check=True,
        )
        files = list(out.iterdir())
        assert len(files) == 1
        assert files[0].read_bytes().startswith(b"c4\x6e\x8e\x40")
        assert ".inflated.bin" in files[0].name

if __name__ == "__main__":
    main()
