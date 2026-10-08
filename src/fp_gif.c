#include "fp_gif.h"
#include <string.h>

static uint64_t rd64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=((uint64_t)p[i])<<(i*8);return v;}
static float f32(uint32_t v){float f;memcpy(&f,&v,4);return f;}

static void reg64(uint8_t reg,uint64_t v,TsFpGifState *s,TsFpGifSummary *o,TsFpGifVertex *vs,size_t cap){
 if(reg==0){s->primitive=(uint32_t)v&0x7ffu;s->primitive_valid=1;o->primitive=s->primitive;}
 else if(reg==1){s->r=v;s->g=v>>8;s->b=v>>16;s->a=v>>24;}
 else if(reg==2){s->s=f32((uint32_t)v);s->t=f32((uint32_t)(v>>32));}
 else if(reg==3){s->s=(float)(v&0x3fffu)/16.0f;s->t=(float)((v>>16)&0x3fffu)/16.0f;}
 else if(reg==4||reg==5){
    if(o->vertices<cap&&vs){
        TsFpGifVertex *x=&vs[o->vertices];
        x->x=(float)(uint16_t)(v&0xffffu)/16.0f;
        x->y=(float)(uint16_t)((v>>16)&0xffffu)/16.0f;
        x->z=(float)(uint32_t)((reg==4)?(v>>32)&0x00ffffffu:(v>>32));
        x->s=s->s;x->t=s->t;x->r=s->r;x->g=s->g;x->b=s->b;x->a=s->a;x->skip=0;x->primitive=s->primitive_valid?(uint8_t)(s->primitive&7u):0xffu;
    }
    o->vertices++;
 }
}

int tsfp_gif_parse_state(const uint8_t *data,size_t size,TsFpGifSummary *out,
                         TsFpGifVertex *vertices,size_t vertex_capacity,TsFpGifState *state){
 size_t p=0;
 if(!data||!out||!state)return -1;
 memset(out,0,sizeof(*out));
 while(p+16<=size){
    uint64_t lo=rd64(data+p),hi=rd64(data+p+8);
    uint32_t nloop=lo&0x7fffu;
    uint8_t flg=(lo>>58)&3u,nreg=(lo>>60)&15u;
    uint8_t pre=(lo>>46)&1u;
    if(!nreg)nreg=16;
    out->tags++;
    if(pre){state->primitive=(lo>>47)&0x7ffu;state->primitive_valid=1;}
    out->primitive=state->primitive;
    out->format=flg;
    out->registers=nreg;
    p+=16;
    if(!nloop){
        if(lo&(1ull<<15))break;
        continue;
    }
    if(flg==0){
        for(uint32_t l=0;l<nloop;l++)for(uint8_t i=0;i<nreg;i++){
            if(p+16>size)return -2;
            uint8_t reg=(hi>>(i*4))&15u;
            uint64_t a=rd64(data+p),b=rd64(data+p+8);
            if(reg==4||reg==5){
                if(out->vertices<vertex_capacity&&vertices){
                    TsFpGifVertex *x=&vertices[out->vertices];
                    x->x=(float)(uint16_t)(a&0xffffu)/16.0f;
                    x->y=(float)(uint16_t)((a>>16)&0xffffu)/16.0f;
                    /* Packed XYZ data carries Z in the low 64-bit register value; b is padding/ADC. */
                    x->z=(float)(uint32_t)((reg==4)?(a>>32)&0x00ffffffu:(a>>32));
                    x->s=state->s;x->t=state->t;x->r=state->r;x->g=state->g;x->b=state->b;x->a=state->a;x->skip=(uint8_t)((b>>47)&1u);x->primitive=state->primitive_valid?(uint8_t)(state->primitive&7u):0xffu;
                }
                out->vertices++;
            } else if(reg==0){
                state->primitive=(uint32_t)a&0x7ffu;
                state->primitive_valid=1;
                out->primitive=state->primitive;
            } else if(reg==1){
                state->r=a;state->g=a>>8;state->b=a>>16;state->a=a>>24;
            } else if(reg==2){
                state->s=f32(a);state->t=f32(a>>32);
            } else if(reg==3){
                state->s=(float)(a&0x3fffu)/16.0f;state->t=(float)((a>>16)&0x3fffu)/16.0f;
            } else if(reg==0x0eu){
                uint8_t ad_reg=(uint8_t)(b&0x7fu);
                reg64(ad_reg,a,state,out,vertices,vertex_capacity);
            }
            p+=16;
        }
        out->loops+=nloop;
    } else if(flg==1){
        uint32_t regs=nloop*nreg,q=(regs+1u)/2u;
        if(p+(size_t)q*16u>size)return -3;
        for(uint32_t i=0;i<q;i++){
            uint64_t a=rd64(data+p),b=rd64(data+p+8),k=i*2;
            if(k<regs)reg64((hi>>((k%nreg)*4))&15u,a,state,out,vertices,vertex_capacity);
            if(k+1<regs)reg64((hi>>(((k+1)%nreg)*4))&15u,b,state,out,vertices,vertex_capacity);
            p+=16;
        }
        out->loops+=nloop;
    } else {
        size_t bytes=(size_t)nloop*16u;
        if(p+bytes>size)return -4;
        p+=bytes;
        out->loops+=nloop;
    }
    if(lo&(1ull<<15))break;
 }
 out->primitive=state->primitive;
 out->bytes_consumed=p;
 return 0;
}

int tsfp_gif_parse(const uint8_t *data,size_t size,TsFpGifSummary *out,TsFpGifVertex *vertices,size_t vertex_capacity){
 TsFpGifState state={0,0,255,255,255,255,0};
 return tsfp_gif_parse_state(data,size,out,vertices,vertex_capacity,&state);
}

static size_t triangulate_one(TsFpGifVertex *dst,size_t capacity,const TsFpGifVertex *src,size_t count,uint32_t primitive){
    if(!dst||!src||!capacity)return 0;
    size_t written=0;
    if(primitive==3u){
        size_t pending[3],used=0;
        for(size_t i=0;i<count;i++){
            if(src[i].skip){used=0;continue;}
            pending[used++]=i;
            if(used==3u){
                if(capacity-written<3u)break;
                dst[written++]=src[pending[0]];
                dst[written++]=src[pending[1]];
                dst[written++]=src[pending[2]];
                used=0;
            }
        }
    }else if(primitive==4u){
        for(size_t i=2;i<count;i++){
            if(src[i].skip)continue;
            if(capacity-written<3u)break;
            if(i&1u){dst[written++]=src[i-1];dst[written++]=src[i-2];dst[written++]=src[i];}
            else {dst[written++]=src[i-2];dst[written++]=src[i-1];dst[written++]=src[i];}
        }
    }else if(primitive==5u){
        for(size_t i=2;i<count;i++){
            if(src[i].skip)continue;
            if(capacity-written<3u)break;
            dst[written++]=src[0];dst[written++]=src[i-1];dst[written++]=src[i];
        }
    }else if(primitive==6u){
        size_t first=0;int have_first=0;
        for(size_t i=0;i<count;i++){
            if(src[i].skip){have_first=0;continue;}
            if(!have_first){first=i;have_first=1;continue;}
            if(capacity-written<6u)break;
            TsFpGifVertex a=src[first],b=src[i],c=a,d=b;
            c.y=b.y;d.x=a.x;
            dst[written++]=a;dst[written++]=b;dst[written++]=c;
            dst[written++]=c;dst[written++]=b;dst[written++]=d;
            have_first=0;
        }
    }
    return written;
}

size_t tsfp_gif_triangulate(TsFpGifVertex *dst,size_t capacity,const TsFpGifVertex *src,size_t count,uint32_t fallback_primitive){
    if(!dst||!src||!capacity)return 0;
    int has_primitive=0;
    for(size_t i=0;i<count;i++)if(src[i].primitive!=0xffu){has_primitive=1;break;}
    if(!has_primitive)return triangulate_one(dst,capacity,src,count,fallback_primitive&7u);
    size_t written=0,start=0;
    while(start<count&&written<capacity){
        uint32_t primitive=src[start].primitive==0xffu?(fallback_primitive&7u):(src[start].primitive&7u);
        size_t end=start+1;
        while(end<count){
            uint32_t next=src[end].primitive==0xffu?(fallback_primitive&7u):(src[end].primitive&7u);
            if(next!=primitive)break;
            end++;
        }
        written+=triangulate_one(dst+written,capacity-written,src+start,end-start,primitive);
        start=end;
    }
    return written;
}
