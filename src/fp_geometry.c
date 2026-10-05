#include "fp_geometry.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t rd16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}

int tsfp_geometry_probe(const uint8_t *data,size_t size,uint32_t table_offset,uint32_t mesh_count,TsFpGeometrySummary *out){
    uint32_t i,j,ptr,next;
    if(!data||!out||mesh_count==0||mesh_count>4096)return -1;
    if(table_offset>size || mesh_count>(size-table_offset)/4u)return -2;
        memset(out,0,sizeof(*out));
    out->table_offset=table_offset;
    out->mesh_count=mesh_count;
    for(i=0;i<mesh_count;i++){
        ptr=rd32(data+table_offset+i*4u);
        if(!ptr) continue;
        if(ptr>=size || (ptr&3u)){out->invalid_count++; continue;}
        next=(uint32_t)size;
        for(j=i+1;j<mesh_count;j++){
            uint32_t q=rd32(data+table_offset+j*4u);
            if(q>ptr && q<next) next=q;
        }
        if(next==(uint32_t)size) next=ptr+8u;
        if(next<=ptr || (next-ptr)%8u){out->invalid_count++; continue;}
        for(uint32_t p=ptr;p<next;p+=8u){
            uint32_t off=rd32(data+p);
            uint16_t count=rd16(data+p+4);
            if(!count) continue;
            if(off>=size || count>(size-off)/16u){out->invalid_count++; continue;}
            out->submesh_count++;
        }
    }
    return 0;
}
