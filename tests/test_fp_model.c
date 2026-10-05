#include "fp_model.h"
#include <assert.h>
#include <stdint.h>
int main(void){
 uint8_t d[128]={0}; TsFpModelHeader h;
 d[0]=0x40; d[4]=0x60; d[8]=0x70;
 assert(tsfp_model_probe(d,sizeof(d),&h)==0);
 assert(h.chunk_a==0x40 && h.chunk_b==0x60 && h.chunk_c==0x70);
 return 0;
}
