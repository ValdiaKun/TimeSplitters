#include "fp_vif.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static void wr32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static int32_t sx(uint32_t v, unsigned bits) {
    uint32_t m = 1u << (bits - 1u);
    return (int32_t)((v ^ m) - m);
}
static unsigned vn(uint8_t f) { return ((f >> 2) & 3u) + 1u; }
static unsigned vl(uint8_t f) { return f & 3u; }

static uint32_t unpack_one(const uint8_t *src, uint8_t format, int uns, uint32_t *out) {
    unsigned n=vn(format), l=vl(format);
    if ((format & 0xfu) == 0xfu) {
        uint32_t x=rd32(src);
        uint32_t v[4];
        v[0]=(x&0x001fu)<<3;
        v[1]=(x&0x03e0u)>>2;
        v[2]=(x&0x7c00u)>>7;
        v[3]=(x&0x8000u)>>8;
        for(unsigned i=0;i<4;i++) out[i]=v[i];
        return 4;
    }
    unsigned bits=32u>>l, bytes=(bits*n+7u)/8u;
    uint32_t v[4]={0,0,0,0};
    if(bits==32){
        for(unsigned i=0;i<n;i++)v[i]=rd32(src+i*4);
    } else if(bits==16){
        for(unsigned i=0;i<n;i++){uint32_t x=rd16(src+i*2);v[i]=uns?x:(uint32_t)sx(x,16);}
    } else {
        for(unsigned i=0;i<n;i++){uint32_t x=src[i];v[i]=uns?x:(uint32_t)sx(x,8);}
    }
    if(n==1){out[0]=out[1]=out[2]=out[3]=v[0];}
    else if(n==2){out[0]=out[2]=v[0];out[1]=out[3]=v[1];}
    else {for(unsigned i=0;i<4;i++)out[i]=v[i];}
    /* VIF consumes complete 32-bit words even when a vector's packed payload
       is narrower than one or more words.  This matters for V3-16/V3-8/V2-8
       streams: the next vector starts on the next 32-bit word boundary. */
    return (bytes + 3u) & ~3u;
}

int tsfp_vif_unpack_memory(const uint8_t *data, size_t size,
                           uint8_t *vu_memory, size_t vu_size,
                           TsFpVifMemorySummary *out) {
    size_t p=0;
    uint32_t addr=0, tops=0, base=0, offset=0, itops=0;
    uint32_t row[4]={0,0,0,0}, col[4]={0,0,0,0};
    uint32_t mask=0;
    uint16_t cl=1, wl=1; uint8_t mode=0;
    uint32_t cycle_pos=0;

    if (!data || !vu_memory || !out || vu_size < 16) return -1;
    memset(out,0,sizeof(*out));
    memset(vu_memory,0,vu_size);

    while (p+4<=size) {
        uint32_t w=rd32(data+p); p+=4;
        uint8_t cmd=(uint8_t)(w>>24), num=(uint8_t)(w>>16);
        uint16_t imm=(uint16_t)w;

        if (cmd==0) continue;
        if (cmd==0x01) {
            cl=(uint8_t)(imm&0xffu);
            wl=(uint8_t)(imm>>8);
            if (!cl) cl=256;
            if (!wl) wl=256;
            cycle_pos=0;
            continue;
        }
        if (cmd==0x02) { offset=(uint32_t)(imm&0x3ffu)*16u; continue; }
        if (cmd==0x03) { base=(uint32_t)(imm&0x3ffu)*16u; continue; }
        if (cmd==0x04) { itops=(uint32_t)(imm&0x3ffu); continue; }
        if (cmd==0x05) { mode=(uint8_t)(imm&3u); continue; }
        if (cmd==0x06 || cmd==0x07 || cmd==0x10 || cmd==0x11 || cmd==0x13 || cmd==0x17) continue;
        if (cmd==0x14 || cmd==0x15) { out->mscal_address=imm; continue; }
        if (cmd==0x20) {
            if(p+4>size)return -2;
            mask=rd32(data+p); p+=4; continue;
        }
        if (cmd==0x30 || cmd==0x31) {
            if(p+16>size)return -3;
            for(unsigned i=0;i<4;i++) {
                uint32_t x=rd32(data+p+i*4);
                if(cmd==0x30) row[i]=x; else col[i]=x;
            }
            p+=16; continue;
        }
        if ((cmd&0xe0u)==0x60u) {
            uint8_t f=cmd&0x0fu;
            addr=(size_t)(imm&0x03ffu)*16u;
            if(imm&0x8000u) addr+=(size_t)tops*16u;
            unsigned n=num ? num : 256;
            unsigned bits=(f==0xfu)?20u:(32u>>vl(f))*vn(f);
            unsigned bytes=(bits+7u)/8u;
            if (!bits) return -4;

            for(unsigned v=0;v<n;v++) {
                if(p+bytes>size)return -5;
                uint32_t q[4];
                (void)unpack_one(data+p,f,(imm&0x4000u)!=0,q);
                p+=bytes;

                size_t target=(size_t)addr;
                for(unsigned i=0;i<4;i++) {
                    unsigned cycle_slot=cycle_pos<4u?cycle_pos:3u;
                    unsigned mask_index=cycle_slot*4u+i;
                    unsigned m=((cmd&0x10u)!=0u)?((mask>>(mask_index*2u))&3u):0u;
                    if(m==0u) {
                        if(f==0xfu) { /* V4-5 ignores STMOD. */ }
                        else if(mode==1u) q[i]+=row[i];
                        else if(mode==2u) { q[i]+=row[i]; row[i]=q[i]; }
                        else if(mode==3u) { row[i]=q[i]; }
                    } else if(m==1u) {
                        q[i]=row[i];
                    } else if(m==2u) {
                        q[i]=col[cycle_slot];
                    }
                    if(m!=3u) {
                        if(target+4u>vu_size)return -6;
                        wr32(vu_memory+target+i*4u,q[i]);
                    }
                }

                if(target+16u>vu_size)return -6;
                addr+=16u;
                out->qwords_written++;
                cycle_pos++;
                if(cycle_pos>=cl) cycle_pos=0;
                if(cl>wl && cycle_pos==wl) {
                    addr+=(size_t)(cl-wl)*16u;
                    cycle_pos=0;
                }
            }
            out->unpack_commands++;
            out->bytes_written=out->qwords_written*16u;
            continue;
        }
        if (cmd==0x4a) {
            uint32_t bytes=(uint32_t)(num?num:256u)*8u;
            if(p+bytes>size)return -7;
            p+=bytes; continue;
        }
        if (cmd==0x50 || cmd==0x51) {
            uint32_t bytes=(uint32_t)imm*16u;
            if(p+bytes>size)return -8;
            p+=bytes; continue;
        }
        (void)base;
        (void)offset;
        (void)itops;
        return -9;
    }
    return p==size ? 0 : -10;
}
