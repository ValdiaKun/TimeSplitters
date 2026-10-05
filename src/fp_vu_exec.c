#include "fp_vu.h"
#include <math.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static float f32(uint32_t v) {
    float x; memcpy(&x,&v,sizeof(x)); return x;
}
static uint32_t u32(float x) {
    uint32_t v; memcpy(&v,&x,sizeof(v)); return v;
}
static int32_t sx11(uint32_t v) {
    v &= 0x7ffu;
    return (v & 0x400u) ? (int32_t)(v | 0xfffff800u) : (int32_t)v;
}
static void set_v(TsFpVuState *s, unsigned d, unsigned mask,
                  const float x[4]) {
    if (!d) return;
    for (unsigned i=0;i<4;i++)
        if (mask & (1u<<i)) s->vf[d][i]=u32(x[i]);
}
static float bc_value(const TsFpVuState *s, unsigned ft, unsigned bc) {
    return f32(s->vf[ft][bc & 3u]);
}
static void upper_exec(TsFpVuState *s, uint32_t up) {
    unsigned ft=(up>>16)&31u, fs=(up>>11)&31u, fd=(up>>6)&31u;
    unsigned mask=(up>>21)&15u, op=up&63u;
    float r[4];
    const float *a=(const float*)s->vf[fs];
    (void)a;
    if (op < 0x20u && op != 0x1cu && op != 0x1eu) {
        unsigned bc=op&3u;
        unsigned kind=(op>>2)&7u;
        float b;
        for(unsigned i=0;i<4;i++) r[i]=f32(s->vf[fs][i]);
        if(op>=0x18u && op<=0x1bu) bc=op&3u;
        if(op==0x1cu || op==0x1du || op==0x1eu || op==0x1fu ||
           op>=0x20u) {
            if(op==0x1cu || op==0x20u || op==0x21u || op==0x24u || op==0x25u) b=f32(s->q);
            else b=f32(s->vi[21]);
        } else b=bc_value(s,ft,bc);
        for(unsigned i=0;i<4;i++) {
            float x=r[i];
            switch(kind) {
            case 0: r[i]=x+b; break;
            case 1: r[i]=x-b; break;
            case 2: r[i]=f32(s->acc[i])+x*b; break;
            case 3: r[i]=f32(s->acc[i])-x*b; break;
            case 4: r[i]=fmaxf(x,b); break;
            case 5: r[i]=fminf(x,b); break;
            case 6: r[i]=x*b; break;
            case 7: r[i]=fminf(x,b); break;
            }
        }
        if(op==0x1du) for(unsigned i=0;i<4;i++) r[i]=fmaxf(f32(s->vf[fs][i]),f32(s->vi[21]));
        set_v(s,fd,mask,r);
        return;
    }
    switch(op) {
    case 0x28: case 0x2c: case 0x2a: case 0x2b: case 0x2f:
    case 0x29: case 0x2d: case 0x2e:
        for(unsigned i=0;i<4;i++) {
            float x=f32(s->vf[fs][i]), y=f32(s->vf[ft][i]);
            if(op==0x28) r[i]=x+y;
            else if(op==0x2c) r[i]=x-y;
            else if(op==0x2a) r[i]=x*y;
            else if(op==0x2b) r[i]=fmaxf(x,y);
            else if(op==0x2f) r[i]=fminf(x,y);
            else if(op==0x29) r[i]=f32(s->acc[i])+x*y;
            else if(op==0x2d) r[i]=f32(s->acc[i])-x*y;
            else r[i]=f32(s->acc[i])-f32(s->vf[fs][(i+1)%3])*f32(s->vf[ft][(i+2)%3]);
        }
        set_v(s,fd,mask,r);
        return;
    case 0x20: case 0x22: case 0x24: case 0x26:
    case 0x21: case 0x23: case 0x25: case 0x27:
        {
            float b = (op&1u) ? f32(s->vi[21]) : f32(s->q);
            for(unsigned i=0;i<4;i++) {
                float x=f32(s->vf[fs][i]);
                if(op==0x20 || op==0x22) r[i]=x+b;
                else if(op==0x24 || op==0x26) r[i]=x-b;
                else if(op==0x21 || op==0x23) r[i]=f32(s->acc[i])+x*b;
                else r[i]=f32(s->acc[i])-x*b;
            }
            set_v(s,fd,mask,r);
        }
        return;
    case 0x1c: case 0x1e:
        {
            float b=(op==0x1c)?f32(s->q):f32(s->vi[21]);
            for(unsigned i=0;i<4;i++) r[i]=f32(s->vf[fs][i])*b;
            set_v(s,fd,mask,r);
        }
        return;
    case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        {
            unsigned group=(fd>>3)&3u, sub=(up>>6)&31u;
            if(sub==4u || sub==5u) {
                unsigned scale = group==0u ? 0u : group==1u ? 4u : group==2u ? 12u : 15u;
                for(unsigned i=0;i<4;i++) {
                    if(!(mask&(1u<<i))) continue;
                    float x=f32(s->vf[fs][i]);
                    if(group==0u && sub==5u) s->vf[ft][i]=(uint32_t)(int32_t)x;
                    else if(group==1u && sub==5u) s->vf[ft][i]=(uint32_t)(int32_t)(x*16.0f);
                    else if(group==2u && sub==5u) s->vf[ft][i]=(uint32_t)(int32_t)(x*4096.0f);
                    else if(group==3u && sub==5u) s->vf[ft][i]=(uint32_t)(int32_t)(x*32768.0f);
                    else s->vf[ft][i]=(uint32_t)((int32_t)f32(s->vf[fs][i])/(1u<<scale));
                }
                return;
            }
            if(group==1u && sub==7u) {
                for(unsigned i=0;i<4;i++) if(mask&(1u<<i)) s->vf[ft][i]=s->vf[fs][i]&0x7fffffffu;
                return;
            }
            if(group==3u && sub==11u) return;
            s->unsupported++;
        }
        return;
    default:
        s->unsupported++;
        return;
    }
}

static void lower_exec(TsFpVuState *s, uint32_t lo, uint32_t next_pc) {
    unsigned op=(lo>>25)&0x7fu, ft=(lo>>16)&31u, fs=(lo>>11)&31u, fd=(lo>>6)&31u;
    int32_t imm=sx11(lo);
    if(op==0x00u || op==0x01u) {
        size_t q=((uint32_t)(s->vi[fs]+imm)&0x3ffu)*16u;
        if(q+16>s->memory_size) { s->unsupported++; return; }
        if(op==0x00u) memcpy(s->vf[ft],s->memory+q,16);
        else memcpy(s->memory+q,s->vf[ft],16);
        return;
    }
    if(op==0x04u || op==0x05u) {
        size_t a=((uint32_t)(s->vi[fs]+imm)&0x3ffu)*16u;
        unsigned word=(lo>>6)&3u;
        if(a+word*4u+4u>s->memory_size) { s->unsupported++; return; }
        if(op==0x04u) s->vi[ft]=rd32(s->memory+a+word*4u);
        else wr32(s->memory+a+word*4u,s->vi[ft]);
        return;
    }
    if(op==0x08u || op==0x09u) {
        if(ft) s->vi[ft]=(uint32_t)(s->vi[fs]+(uint32_t)(lo&0x7ffu)*(op==0x08u?1u:-1u));
        return;
    }
    if(op==0x30u) {
        if(ft) memcpy(s->vf[ft],s->vf[fs],16);
        return;
    }
    if(op==0x40u) { if(fd) s->vi[fd]=s->vi[fs]+s->vi[ft]; return; }
    if(op==0x41u) { if(fd) s->vi[fd]=s->vi[fs]-s->vi[ft]; return; }
    if(op==0x44u) { if(fd) s->vi[fd]=s->vi[fs]&s->vi[ft]; return; }
    if(op==0x45u) { if(fd) s->vi[fd]=s->vi[fs]|s->vi[ft]; return; }
    if(op==0x20u) { s->pc=(uint32_t)((int32_t)next_pc+imm); return; }
    if(op==0x21u) { if(fd) s->vi[fd]=next_pc+1u; s->pc=(uint32_t)((int32_t)next_pc+imm); return; }
    if(op==0x24u || op==0x25u || op==0x28u || op==0x29u ||
       op==0x2au || op==0x2bu) {
        int32_t a=(int32_t)s->vi[fs], b=(int32_t)s->vi[ft];
        int take=(op==0x24u)?a==b:(op==0x25u)?a!=b:(op==0x28u)?a<0:(op==0x29u)?a>0:(op==0x2au)?a<=0:a>=0;
        if(take) s->pc=(uint32_t)((int32_t)next_pc+imm);
        return;
    }
    if(op==0x6cu) {
        size_t a=((size_t)s->vi[fs]&0x3ffu)*16u;
        s->xgkick_pc=s->pc;
        if(a<s->memory_size && s->gif && s->gif_used<s->gif_size) {
            size_t n=s->memory_size-a;
            if(n>s->gif_size-s->gif_used) n=s->gif_size-s->gif_used;
            memcpy(s->gif+s->gif_used,s->memory+a,n);
            s->gif_used+=n;
        }
        return;
    }
    s->unsupported++;
}

void tsfp_vu_state_init(TsFpVuState *state, uint8_t *memory, size_t memory_size,
                        uint8_t *gif, size_t gif_size) {
    if(!state) return;
    memset(state,0,sizeof(*state));
    state->memory=memory; state->memory_size=memory_size;
    state->gif=gif; state->gif_size=gif_size;
    state->xgkick_pc=UINT32_MAX;
}

int tsfp_vu_execute(const uint8_t *micro, size_t size, uint32_t start,
                    TsFpVuState *state, uint32_t max_steps) {
    if(!micro || !state || (size&7u) || start>=size/8u) return -1;
    state->pc=start;
    for(state->steps=0; state->steps<max_steps && state->pc<size/8u; state->steps++) {
        uint32_t pc=state->pc;
        uint32_t lo=rd32(micro+pc*8u), up=rd32(micro+pc*8u+4u);
        state->pc=pc+1u;
        lower_exec(state,lo,state->pc);
        upper_exec(state,up);
        if(up&0x40000000u) return 0;
    }
    return state->steps>=max_steps ? -2 : 0;
}
