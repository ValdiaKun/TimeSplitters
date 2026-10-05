#include "fp_model.h"
#include <string.h>
#include <math.h>
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float rf32(const uint8_t *p){uint32_t u=rd32(p); float f; memcpy(&f,&u,4); return f;}
int tsfp_model_probe(const uint8_t *data,size_t size,TsFpModelHeader *out){
 uint32_t a,b,c,count;
 float scale;
 if(!data||!out||size<16)return -1;
 a=rd32(data); b=rd32(data+4); c=rd32(data+8);
 if(a>=size||b>=size||c>=size)return -2;
 if((a|b|c)&3u)return -3;
 if(a==b||a==c||b==c)return -4;
 if(b+40>size)return -5;
 count=rd32(data+b);
 scale=rf32(data+b+36);
 if(count==0||count>4096||!isfinite(scale)||scale==0.0f)return -6;
 memset(out,0,sizeof(*out));
 out->material_offset=a; out->info_offset=b; out->auxiliary_offset=c;
 out->mesh_count=count; out->scale=scale;
 return 0;
}
