#!/usr/bin/env python3
"""Inspect Future Perfect CHR VIF packets and their VU microprogram target.

This tool consumes user-supplied game assets locally; it does not embed assets
in the repository. It reports packet structure and the MSCAL entry/exit point.
"""
import argparse
import struct
from collections import Counter

def rd32(b, o):
    return struct.unpack_from("<I", b, o)[0]

def vutext_from_elf(blob):
    if blob[:4] != b"\x7fELF" or blob[4:6] != b"\x01\x01":
        raise ValueError("expected 32-bit little-endian ELF")
    shoff = rd32(blob, 32)
    shentsz, shnum, shstr = struct.unpack_from("<HHH", blob, 46)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsz
        name, _, _, _, off, size = struct.unpack_from("<IIIIII", blob, o)
        secs.append((name, off, size))
    no, so, ss = secs[shstr]
    names = blob[so:so + ss]
    for name, off, size in secs:
        e = names.find(b"\0", name)
        if e >= 0 and names[name:e] == b".vutext":
            return blob[off:off + size]
    raise ValueError(".vutext not found")

def scan_vif(d):
    p = 0
    formats = Counter()
    mscal = []
    packets = 0
    while p < len(d):
        if p + 4 > len(d):
            raise ValueError("truncated VIF command")
        w = rd32(d, p)
        cmd, num, imm = w >> 24, (w >> 16) & 0xff, w & 0xffff
        p += 4
        if cmd == 0:
            continue
        if cmd == 1:
            if p == 4 or p > len(d):
                pass
            continue
        if cmd in (2, 3, 4, 5, 6, 7, 0x10, 0x11, 0x13, 0x17):
            continue
        if cmd in (0x14, 0x15):
            mscal.append(imm)
            packets += 1
            continue
        if cmd == 0x20:
            p += 4
            continue
        if cmd in (0x30, 0x31):
            p += 16
            continue
        if 0x60 <= cmd <= 0x7f:
            f = cmd & 0xf
            bits = 20 if f == 0xf else (32 >> (f & 3)) * (((f >> 2) & 3) + 1)
            words = (bits + 31) // 32
            n = num or 256
            consumed = n * words * 4
            if p + consumed > len(d):
                raise ValueError("truncated UNPACK payload")
            formats[f] += 1
            p += consumed
            continue
        if cmd == 0x4a:
            p += (num or 256) * 8
            continue
        if cmd in (0x50, 0x51):
            p += imm * 16
            continue
        raise ValueError(f"unknown VIF command 0x{cmd:02x} at 0x{p-4:x}")
    if p != len(d):
        raise ValueError(f"packet ended at 0x{p:x}, expected 0x{len(d):x}")
    return packets, mscal, formats

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("chr_pak")
    ap.add_argument("boot_elf")
    ap.add_argument("--entry-offset", type=lambda x: int(x, 0), default=0x800)
    ap.add_argument("--entry-length", type=lambda x: int(x, 0), default=112912)
    args = ap.parse_args()

    pak = open(args.chr_pak, "rb").read()
    model = pak[args.entry_offset:args.entry_offset + args.entry_length]
    mesh_table = rd32(model, 0x1a30 + 24)
    mesh_count = rd32(model, 0x1a30 + 12)
    ptrs = [rd32(model, mesh_table + i * 4) for i in range(mesh_count)]

    total = 0
    targets = Counter()
    for i, ptr in enumerate(ptrs):
        if not ptr:
            continue
        later = [q for q in ptrs[i + 1:] if q > ptr]
        end = min(later) if later else ptr + 8
        for q in range(ptr, end, 8):
            off = rd32(model, q)
            count = struct.unpack_from("<H", model, q + 4)[0]
            if not count:
                continue
            total += 1
            packets, mscal, formats = scan_vif(model[off:off + count * 16])
            targets.update(mscal)

    vutext = vutext_from_elf(open(args.boot_elf, "rb").read())
    for target in sorted(targets):
        pc = target
        end = None
        while pc * 8 + 8 <= len(vutext):
            _, upper = struct.unpack_from("<II", vutext, pc * 8)
            if upper & 0x40000000:
                end = pc
                break
            pc += 1
        print(f"MSCAL 0x{target:03x}: uses={targets[target]} E=0x{end:03x}" if end is not None
              else f"MSCAL 0x{target:03x}: uses={targets[target]} E=not-found")
    print(f"valid_submeshes={total}")
    print(f"unique_mscal_targets={len(targets)}")

if __name__ == "__main__":
    main()
