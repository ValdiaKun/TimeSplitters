#!/usr/bin/env python3
"""Inspect a PS2 ELF VU microprogram without embedding game assets.

Usage:
  inspect_vu_micro.py BOOT.ELF --address 0x683
"""
import argparse
import struct

def read_vutext(blob):
    if blob[:4] != b"\x7fELF" or blob[4] != 1 or blob[5] != 1:
        raise ValueError("expected 32-bit little-endian ELF")
    e_shoff = struct.unpack_from("<I", blob, 32)[0]
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 46)
    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        name, _, _, _, sh_offset, sh_size = struct.unpack_from("<IIIIII", blob, off)
        sections.append((name, sh_offset, sh_size))
    shoff = e_shoff + e_shstrndx * e_shentsize
    _, _, _, _, stroff, strsize = struct.unpack_from("<IIIIII", blob, shoff)
    strings = blob[stroff:stroff + strsize]
    def s(n):
        end = strings.find(b"\0", n)
        return strings[n:end].decode("ascii", "replace")
    for name, off, size in sections:
        if s(name) == ".vutext":
            return blob[off:off + size]
    raise ValueError(".vutext section not found")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--address", type=lambda x: int(x, 0), default=0x683)
    ap.add_argument("--max", type=lambda x: int(x, 0), default=0x100)
    args = ap.parse_args()
    text = read_vutext(open(args.elf, "rb").read())
    for pc in range(args.address, min(args.address + args.max, len(text) // 8)):
        lo, up = struct.unpack_from("<II", text, pc * 8)
        flags = []
        if up & 0x80000000: flags.append("I")
        if up & 0x40000000: flags.append("E")
        if up & 0x20000000: flags.append("M")
        if up & 0x10000000: flags.append("D")
        if up & 0x08000000: flags.append("T")
        print(f"{pc:04x}: {lo:08x} {up:08x}" + (f"  [{''.join(flags)}]" if flags else ""))
        if up & 0x40000000:
            break

if __name__ == "__main__":
    main()
