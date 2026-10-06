#include "fp_gif.h"
#include <string.h>

static uint64_t rd64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=((uint64_t)p[i])<<(i*8);return v;}
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float f32(uint32_t v){float f;memcpy(&f,&v,4);return f;}

typedef struct {float s,t;uint8_t r,g,b,a;} GifState;
static void reg64(uint8_t reg,uint64_t v,GifState *s,TsFpGifSummary *o,TsFpGifVertex *vs,size_t cap){
 if(reg==1){s->r=v;s->g=v>>8;s->b=v>>16;s->a=v>>24;}
 else if(reg==2){s->s=f32((uint32_t)v);s->t=f32((uint32_t)(v>>32));}
 else if(reg==3){s->s=(float)(v&0x3fffu)/16.0f;s->t=(float)((v>>16)&0x3fffu)/16.0f;}
 else if(reg==5){if(o->vertices<cap&&vs){TsFpGifVertex *x=&vs[o->vertices];x->x=(float)(int16_t)(v&0xffffu)/16.0f;x->y=(float)(int16_t)((v>>32)&0xffffu)/16.0f;x->z=0;x->s=s->s;x->t=s->t;x->r=s->r;x->g=s->g;x->b=s->b;x->a=s->a;}o->vertices++;}
}

int tsfp_gif_parse(const uint8_t *data,size_t size,TsFpGifSummary *out,TsFpGifVertex *vertices,size_t vertex_capacity){
 size_t p=0;if(!data||!out)return -1;memset(out,0,sizeof(*out));GifState s={0,0,255,255,255,255};
 while(p+16<=size){uint64_t lo=rd64(data+p),hi=rd64(data+p+8);uint32_t nloop=lo&0x7fffu;uint8_t flg=(lo>>58)&3u,nreg=(lo>>60)&15u;if(!nreg)nreg=16;out->tags++;out->primitive=(lo>>47)&0x7ffu;out->format=flg;out->registers=nreg;p+=16;if(!nloop){if(lo&(1ull<<15))break;continue;}
 if(flg==0){for(uint32_t l=0;l<nloop;l++)for(uint8_t i=0;i<nreg;i++){if(p+16>size)return -2;uint8_t reg=(hi>>(i*4))&15u;uint64_t a=rd64(data+p),b=rd64(data+p+8);if(reg==5){if(out->vertices<vertex_capacity&&vertices){TsFpGifVertex *x=&vertices[out->vertices];x->x=(float)(int16_t)(a&0xffffu)/16.0f;x->y=(float)(int16_t)((a>>32)&0xffffu)/16.0f;x->z=f32((uint32_t)b);x->s=s.s;x->t=s.t;x->r=s.r;x->g=s.g;x->b=s.b;x->a=s.a;}out->vertices++;}else if(reg==1){s.r=a;s.g=a>>8;s.b=a>>16;s.a=a>>24;}else if(reg==2){s.s=f32(a);s.t=f32(a>>32);}else if(reg==3){s.s=(float)(a&0x3fffu)/16.0f;s.t=(float)((a>>16)&0x3fffu)/16.0f;}p+=16;}out->loops+=nloop;}
 else if(flg==1){uint32_t regs=nloop*nreg,q=(regs+1u)/2u;if(p+(size_t)q*16u>size)return -3;for(uint32_t i=0;i<q;i++){uint64_t a=rd64(data+p),b=rd64(data+p+8),k=i*2;if(k<regs)reg64((hi>>((k%nreg)*4))&15u,a,&s,out,vertices,vertex_capacity);if(k+1<regs)reg64((hi>>(((k+1)%nreg)*4))&15u,b,&s,out,vertices,vertex_capacity);p+=16;}out->loops+=nloop;}
 else {size_t bytes=(size_t)nloop*16u;if(p+bytes>size)return -4;p+=bytes;out->loops+=nloop;}
 if(lo&(1ull<<15))break;}
 out->bytes_consumed=p;return 0;
}
