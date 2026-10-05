#include "fp_model.h"
#include <string.h>
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int tsfp_model_probe(const uint8_t *data,size_t size,TsFpModelHeader *out){
 uint32_t a,b,c,flags;
 if(!data||!out||size<16)return -1;
 a=rd32(data);b=rd32(data+4);c=rd32(data+8);flags=rd32(data+12);
 if((a|b|c)&3u)return -2;
 if(a>=size||b>=size||c>=size)return -3;
 if(a<0x40||b<0x40||c<0x40)return -4;
 if(flags!=0)return -5;
 if(a==b||a==c||b==c)return -6;
 memset(out,0,sizeof(*out));out->chunk_a=a;out->chunk_b=b;out->chunk_c=c;out->flags=flags;return 0;
}
