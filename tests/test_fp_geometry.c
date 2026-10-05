#include "fp_geometry.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
int main(void){
    uint8_t d[512]={0}; TsFpGeometrySummary s;
    /* Two mesh pointers. Mesh 0 has two submeshes; mesh 1 has one. */
    memcpy(d+0x20,&(uint32_t){0x40},4);
    memcpy(d+0x24,&(uint32_t){0x50},4);
    memcpy(d+0x28,&(uint32_t){0x200},4);
    memcpy(d+0x40,&(uint32_t){0x100},4);
    memcpy(d+0x44,&(uint16_t){2},2);
    memcpy(d+0x46,&(uint16_t){0x16},2);
    memcpy(d+0x48,&(uint32_t){0x140},4);
    memcpy(d+0x4c,&(uint16_t){1},2);
    memcpy(d+0x4e,&(uint16_t){7},2);
    memcpy(d+0x50,&(uint32_t){0x180},4);
    memcpy(d+0x54,&(uint16_t){3},2);
    assert(tsfp_geometry_probe(d,sizeof(d),0x20,3,&s)==0);
    assert(s.mesh_count==3 && s.submesh_count==3 && s.invalid_count==0);
    return 0;
}
