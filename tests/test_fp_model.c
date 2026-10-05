#include "fp_model.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void){
    uint8_t d[640]={0};
    TsFpModelHeader h;
    uint32_t count=41, mats=29, lods=1;
    float scale=0.94f;

    memcpy(d+0,&(uint32_t){0x50},4);
    memcpy(d+4,&(uint32_t){0x40},4);
    memcpy(d+8,&(uint32_t){0x70},4);
    memcpy(d+0x40,&count,4);
    memcpy(d+0x44,&mats,4);
    memcpy(d+0x48,&lods,4);
    memcpy(d+0x40+36,&scale,4);

    assert(tsfp_model_probe(d,sizeof(d),&h)==0);
    assert(h.material_offset==0x50 && h.info_offset==0x40 && h.auxiliary_offset==0x70);
    assert(h.mesh_count==41 && h.material_count==29 && h.lod_count==1);
    assert(h.scale==scale);
    return 0;
}
