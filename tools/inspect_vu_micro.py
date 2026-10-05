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
        upper = {
            0:"ADDx",1:"ADDy",2:"ADDz",3:"ADDw",4:"SUBx",5:"SUBy",6:"SUBz",7:"SUBw",
            8:"MADDx",9:"MADDy",10:"MADDz",11:"MADDw",12:"MSUBx",13:"MSUBy",14:"MSUBz",15:"MSUBw",
            16:"MAXx",17:"MAXy",18:"MAXz",19:"MAXw",20:"MINIx",21:"MINIy",22:"MINIz",23:"MINIw",
            24:"MULx",25:"MULy",26:"MULz",27:"MULw",28:"MULq",29:"MAXi",30:"MULi",31:"MINIi",
            32:"ADDq",33:"MADDq",34:"ADDi",35:"MADDi",36:"SUBq",37:"MSUBq",38:"SUBi",39:"MSUBi",
            40:"ADD",41:"MADD",42:"MUL",43:"MAX",44:"SUB",45:"MSUB",46:"OPMSUB",47:"MINI"
        }
        lower = {
            0:"LQ",1:"SQ",4:"ILW",5:"ISW",8:"IADDIU",9:"ISUBIU",
            16:"FCEQ",17:"FCSET",18:"FCAND",19:"FCOR",20:"FSEQ",21:"FSSET",22:"FSAND",23:"FSOR",
            24:"FMEQ",26:"FMAND",27:"FMOR",28:"FCGET",32:"B",33:"BAL",36:"JR",37:"JALR",
            40:"IBEQ",41:"IBNE",44:"IBLTZ",45:"IBGTZ",46:"IBLEZ",47:"IBGEZ",
            64:"IADD",65:"ISUB",66:"IADDI",68:"IAND",69:"IOR",108:"XGKICK"
        }
        uop = upper.get(up & 0x3f, f"UPPER_{up & 0x3f:02x}")
        lop = lower.get((lo >> 25) & 0x7f, f"LOWER_{(lo >> 25) & 0x7f:02x}")
        ft=(up>>16)&31; fs=(up>>11)&31; fd=(up>>6)&31
        dest=(up>>21)&15
        it=(lo>>16)&15; is_=(lo>>11)&15; imm=lo&0xffff
        args=f"ft{ft},fs{fs},fd{fd},dest={dest:x}" if (up&0x3f)<48 else ""
        if ((lo>>25)&0x7f) in (0,1,4,5): args=f"vi{it},vi{is_},imm={imm if imm<0x8000 else imm-0x10000}"
        elif ((lo>>25)&0x7f)==0x6c: args=f"vi{is_}"
        print(f"{pc:04x}: {lo:08x} {up:08x}  {uop:<8} {lop:<8} {args}" + (f"  [{''.join(flags)}]" if flags else ""))
        if up & 0x40000000:
            break

if __name__ == "__main__":
    main()
