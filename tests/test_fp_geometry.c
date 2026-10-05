#include "fp_geometry.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
int main(void){
    uint8_t d[512]={0}; TsFpGeometrySummary s; TsFpSubmesh v[4];
    memcpy(d+0x20,&(uint32_t){0x40},4);
    memcpy(d+0x24,&(uint32_t){0x50},4);
    memcpy(d+0x40,&(uint32_t){0x100},4);
    memcpy(d+0x44,&(uint16_t){2},2);
    memcpy(d+0x46,&(uint16_t){0x16},2);
    memcpy(d+0x48,&(uint32_t){0x140},4);
    memcpy(d+0x4c,&(uint16_t){1},2);
    memcpy(d+0x4e,&(uint16_t){7},2);
    memcpy(d+0x50,&(uint32_t){0x180},4);
    memcpy(d+0x54,&(uint16_t){3},2);
    assert(tsfp_geometry_probe(d,sizeof(d),0x20,2,&s)==0);
    assert(s.mesh_count==2 && s.submesh_count==3 && s.invalid_count==0);
    assert(tsfp_geometry_collect(d,sizeof(d),0x20,2,v,4)==3);
    assert(v[0].data_offset==0x100 && v[0].vertex_count==2 && v[0].flags==0x16);
    assert(v[1].data_offset==0x140 && v[1].vertex_count==1 && v[1].flags==7);
    assert(v[2].data_offset==0x180 && v[2].vertex_count==3);
    return 0;
}
