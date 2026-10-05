#!/usr/bin/env python3
"""Inspect Future Perfect CHR model mesh/submesh tables without exporting game assets."""
import argparse
import struct
from pathlib import Path

def u32(b,o): return struct.unpack_from("<I",b,o)[0]
def u16(b,o): return struct.unpack_from("<H",b,o)[0]

def inspect(data, model_offset):
    if model_offset < 0 or model_offset + 40 > len(data):
        raise ValueError("model offset outside buffer")
    material_offset=u32(data,model_offset)
    info_offset=u32(data,model_offset+4)
    aux_offset=u32(data,model_offset+8)
    meshes=u32(data,model_offset+0x10)
    mats=u32(data,model_offset+0x14)
    lods=u32(data,model_offset+0x18)
    mesh_table=u32(data,model_offset+0x1c)
    lod_table=u32(data,model_offset+0x20)
    scale=struct.unpack_from("<f",data,model_offset+0x24)[0]
    if not (0 < meshes <= 4096 and mesh_table < len(data)):
        raise ValueError("invalid model metadata")
    subs=[]
    for i in range(meshes):
        p=u32(data,mesh_table+i*4)
        if not p:
            continue
        nxt=None
        for j in range(i+1,meshes):
            q=u32(data,mesh_table+j*4)
            if q>p and (nxt is None or q<nxt):
                nxt=q
        if nxt is None:
            nxt=p+8
        if nxt>len(data) or nxt<=p or (nxt-p)%8:
            continue
        for o in range(p,nxt,8):
            off=u32(data,o); count=u16(data,o+4); flags=u16(data,o+6)
            if count and off + count*16 <= len(data):
                head=struct.unpack_from("<4I",data,off)
                subs.append((i,off,count,flags,head))
    return {
        "material_offset": material_offset, "info_offset": info_offset,
        "auxiliary_offset": aux_offset, "mesh_count": meshes,
        "material_count": mats, "lod_count": lods, "mesh_table_offset": mesh_table,
        "lod_table_offset": lod_table, "scale": scale, "submesh_count": len(subs),
        "submeshes": subs,
    }

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("pak", type=Path)
    ap.add_argument("--entry-offset", type=lambda x:int(x,0), default=None)
    ap.add_argument("--entry-length", type=lambda x:int(x,0), default=None)
    ap.add_argument("--json", action="store_true")
    args=ap.parse_args()
    raw=args.pak.read_bytes()
    if raw[:4] != b"P5CK":
        raise SystemExit("not a P5CK archive")
    index=u32(raw,4)
    if args.entry_offset is None:
        entry_offset=u32(raw,index+4)
        entry_length=u32(raw,index+8)
    else:
        entry_offset=args.entry_offset
        entry_length=args.entry_length or (len(raw)-entry_offset)
    model=raw[entry_offset:entry_offset+entry_length]
    result=inspect(model,0)
    if args.json:
        import json
        print(json.dumps({k:v for k,v in result.items() if k!="submeshes"},indent=2))
    else:
        for k,v in result.items():
            if k!="submeshes": print(f"{k}: {v}")
        for mesh,off,count,flags,head in result["submeshes"][:16]:
            print(f"mesh={mesh:3d} data=0x{off:x} vertices16={count:4d} flags=0x{flags:04x} head=" + " ".join(f"{x:08x}" for x in head))

if __name__=="__main__":
    main()
