#include "fp_geometry.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t rd16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}

static int mesh_range(const uint8_t *data,size_t size,uint32_t table_offset,uint32_t mesh_count,
                      uint32_t index,uint32_t *begin,uint32_t *end){
    uint32_t ptr=rd32(data+table_offset+index*4u), next=(uint32_t)size;
    if(!ptr || ptr>=size || (ptr&3u)) return -1;
    for(uint32_t j=index+1;j<mesh_count;j++){
        uint32_t q=rd32(data+table_offset+j*4u);
        if(q>ptr && q<next) next=q;
    }
    if(next==(uint32_t)size) next=ptr+8u;
    if(next<=ptr || next-ptr<8u || ((next-ptr)&7u) || next>size) return -1;
    *begin=ptr; *end=next; return 0;
}

int tsfp_geometry_probe(const uint8_t *data,size_t size,uint32_t table_offset,uint32_t mesh_count,TsFpGeometrySummary *out){
    if(!data||!out||mesh_count==0||mesh_count>4096)return -1;
    if(table_offset>size || mesh_count>(size-table_offset)/4u)return -2;
    memset(out,0,sizeof(*out)); out->table_offset=table_offset; out->mesh_count=mesh_count;
    for(uint32_t i=0;i<mesh_count;i++){
        uint32_t begin,end;
        if(mesh_range(data,size,table_offset,mesh_count,i,&begin,&end)!=0){
            if(rd32(data+table_offset+i*4u)) out->invalid_count++;
            continue;
        }
        for(uint32_t p=begin;p<end;p+=8u){
            uint32_t off=rd32(data+p); uint16_t count=rd16(data+p+4);
            if(!count) continue;
            if(off>=size || count>(size-off)/16u){out->invalid_count++; continue;}
            out->submesh_count++;
        }
    }
    return 0;
}

size_t tsfp_geometry_collect(const uint8_t *data,size_t size,uint32_t table_offset,uint32_t mesh_count,
                             TsFpSubmesh *out,size_t capacity){
    if(!data||!out||!capacity||mesh_count==0||mesh_count>4096)return 0;
    if(table_offset>size || mesh_count>(size-table_offset)/4u)return 0;
    size_t n=0;
    for(uint32_t i=0;i<mesh_count && n<capacity;i++){
        uint32_t begin,end;
        if(mesh_range(data,size,table_offset,mesh_count,i,&begin,&end)!=0) continue;
        for(uint32_t p=begin;p<end && n<capacity;p+=8u){
            uint32_t off=rd32(data+p); uint16_t count=rd16(data+p+4), flags=rd16(data+p+6);
            if(!count || off>=size || count>(size-off)/16u) continue;
            out[n].data_offset=off; out[n].vertex_count=count; out[n].flags=flags; n++;
        }
    }
    return n;
}
