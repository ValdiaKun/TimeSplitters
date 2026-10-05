#include "fp_model.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
int main(void){
 uint8_t d[128]={0}; TsFpModelHeader h; uint32_t count=41; float scale=0.94f;
 memcpy(d+0,&(uint32_t){0x20},4); memcpy(d+4,&(uint32_t){0x40},4); memcpy(d+8,&(uint32_t){0x60},4);
 memcpy(d+0x40,&count,4); memcpy(d+0x40+36,&scale,4);
 assert(tsfp_model_probe(d,sizeof(d),&h)==0);
 assert(h.material_offset==0x20 && h.info_offset==0x40 && h.auxiliary_offset==0x60);
 assert(h.mesh_count==41 && h.scale>0.9f && h.scale<1.0f);
 return 0;
}
