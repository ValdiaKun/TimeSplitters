#include "fp_model.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void){
    uint8_t d[1280]={0};
    TsFpModelHeader h;
    uint32_t count=41, mats=29, lods=1;
    float scale=0.94f;

    memcpy(d+0,&(uint32_t){0x50},4);
    memcpy(d+4,&(uint32_t){0x40},4);
    memcpy(d+8,&(uint32_t){0x70},4);
    memcpy(d+0x40,&count,4);
    memcpy(d+0x44,&mats,4);
    memcpy(d+0x48,&lods,4);
    memcpy(d+0x40+24,&(uint32_t){0x90},4);
    memcpy(d+0x40+28,&(uint32_t){0x400},4);
    memcpy(d+0x40+36,&scale,4);

    assert(tsfp_model_probe(d,sizeof(d),&h)==0);
    assert(h.material_offset==0x50 && h.info_offset==0x40 && h.auxiliary_offset==0x70);
    assert(h.mesh_count==41 && h.material_count==29 && h.lod_count==1);
    assert(h.mesh_table_offset==0x90 && h.lod_table_offset==0x400);
    assert(h.scale==scale);

    /* The material-table subtraction must not underflow on short buffers. */
    {
        uint8_t small[128]={0};
        uint32_t one=1, many=100;
        float unit_scale=1.0f;
        memcpy(small,&(uint32_t){0x50},4);
        memcpy(small+4,&(uint32_t){0x40},4);
        memcpy(small+8,&(uint32_t){0x70},4);
        memcpy(small+0x40,&one,4);
        memcpy(small+0x44,&many,4);
        memcpy(small+0x48,&one,4);
        memcpy(small+0x40+36,&unit_scale,4);
        assert(tsfp_model_probe(small,sizeof(small),&h)==-6);
        memcpy(small+4,&(uint32_t){0x60},4);
        assert(tsfp_model_probe(small,sizeof(small),&h)==-4);
    }
    return 0;
}
