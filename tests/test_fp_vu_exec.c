#include "fp_vu.h"
#include <assert.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
static uint32_t u32(float x){uint32_t v;memcpy(&v,&x,4);return v;}
static float f32(uint32_t x){float v;memcpy(&v,&x,4);return v;}
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}

static uint32_t upper(uint8_t op, uint8_t fd, uint8_t fs, uint8_t ft, uint8_t mask) {
    return ((uint32_t)op) | ((uint32_t)fd<<6) | ((uint32_t)fs<<11) |
           ((uint32_t)ft<<16) | ((uint32_t)mask<<21);
}
static uint32_t lower(uint8_t op, uint8_t fd, uint8_t fs, uint8_t ft) {
    return ((uint32_t)op<<25) | ((uint32_t)fd<<6) | ((uint32_t)fs<<11) | ((uint32_t)ft<<16);
}
static uint32_t branch(uint8_t op, uint8_t is, uint8_t it, int imm) {
    return ((uint32_t)op<<25) | ((uint32_t)it<<16) | ((uint32_t)is<<11) |
           ((uint32_t)imm & 0x7ffu);
}
static uint32_t flag_imm(uint8_t op, uint8_t it, uint32_t imm) {
    return ((uint32_t)op<<25) | ((uint32_t)it<<16) |
           (imm&0x7ffu) | (((imm>>11)&1u)<<21);
}
int main(void) {
    uint8_t micro[64]={0}, mem[4096]={0}, gif[4096]={0};
    uint32_t w;
    /* vi1 = 2; vi2 = 3; vi3 = vi1 + vi2 */
    w=lower(0x08,0,0,1)|2; memcpy(micro+0,&w,4); w=0; memcpy(micro+4,&w,4);
    w=lower(0x08,0,0,2)|3; memcpy(micro+8,&w,4); w=0; memcpy(micro+12,&w,4);
    w=lower(0x30,3,1,2); memcpy(micro+16,&w,4); w=0; memcpy(micro+20,&w,4);
    w=0; memcpy(micro+24,&w,4); w=upper(0x28,3,1,2,0xf); memcpy(micro+28,&w,4);
    w=0; memcpy(micro+32,&w,4); w=0x40000000u; memcpy(micro+36,&w,4);
    /* vf1/vf2 are initialized directly for the ALU regression. */
    float a[4]={1,2,3,4}, b[4]={5,6,7,8};
    TsFpVuState s; tsfp_vu_state_init(&s,mem,sizeof(mem),gif,sizeof(gif));
    memcpy(s.vf[1],a,16); memcpy(s.vf[2],b,16);
    assert(tsfp_vu_execute(micro,sizeof(micro),0,&s,32)==0);
    assert(s.vi[3]==5u);
    float got[4]; memcpy(got,s.vf[3],sizeof(got));
    assert(got[0]==6.0f && got[1]==8.0f && got[2]==10.0f && got[3]==12.0f);
    {
        /* VU destination mask encoding: bit3=x, bit2=y, bit1=z, bit0=w. */
        uint8_t pm[32]={0}; uint32_t x;
        x=0; memcpy(pm,&x,4); x=upper(0x28,3,1,2,0x8); memcpy(pm+4,&x,4);
        x=0; memcpy(pm+8,&x,4); x=0x40000000u; memcpy(pm+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(1.0f); t.vf[1][1]=u32(2.0f); t.vf[1][2]=u32(3.0f); t.vf[1][3]=u32(4.0f);
        t.vf[2][0]=u32(5.0f); t.vf[2][1]=u32(6.0f); t.vf[2][2]=u32(7.0f); t.vf[2][3]=u32(8.0f);
        assert(tsfp_vu_execute(pm,sizeof(pm),0,&t,8)==0);
        assert(f32(t.vf[3][0])==6.0f && f32(t.vf[3][1])==0.0f &&
               f32(t.vf[3][2])==0.0f && f32(t.vf[3][3])==0.0f);
    }
    {
        uint8_t lm[48]={0};
        float lv[4]={9,10,11,12};
        memcpy(mem+48,lv,16);
        uint32_t q=lower(0,0,0,2)|3; memcpy(lm+0,&q,4);
        q=0x40000000u; memcpy(lm+12,&q,4);
        TsFpVuState l; tsfp_vu_state_init(&l,mem,sizeof(mem),gif,sizeof(gif));
        assert(tsfp_vu_execute(lm,sizeof(lm),0,&l,8)==0);
        float lgot[4]; memcpy(lgot,l.vf[2],sizeof(lgot));
        assert(lgot[0]==9.0f && lgot[1]==10.0f && lgot[2]==11.0f && lgot[3]==12.0f);
    }
    {
        /*
         * Upper/lower halves execute in parallel.  The I-bit immediate is
         * committed after the upper instruction, so ADDi must see the old I.
         */
        uint8_t m[32]={0}; uint32_t x;
        x=0x12345678u; memcpy(m,&x,4);
        x=0x80000000u | upper(0x22,3,1,0,0xf); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[21]=u32(2.0f);
        t.vf[1][0]=u32(1.0f); t.vf[1][1]=u32(1.0f); t.vf[1][2]=u32(1.0f); t.vf[1][3]=u32(1.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.vf[3][0])==3.0f && t.vi[21]==0x12345678u);
    }
    {
        /*
         * A lower store paired with an upper VF write must read the VF value
         * from the beginning of the cycle, not the upper result.
         */
        uint8_t m[32]={0}; uint32_t x;
        x=lower(0x01,0,0,3); memcpy(m,&x,4); x=upper(0x28,3,1,2,0xf); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(2.0f); t.vf[1][1]=u32(2.0f); t.vf[1][2]=u32(2.0f); t.vf[1][3]=u32(2.0f);
        t.vf[2][0]=u32(3.0f); t.vf[2][1]=u32(3.0f); t.vf[2][2]=u32(3.0f); t.vf[2][3]=u32(3.0f);
        t.vf[3][0]=u32(9.0f); t.vf[3][1]=u32(9.0f); t.vf[3][2]=u32(9.0f); t.vf[3][3]=u32(9.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        float stored[4]; memcpy(stored,mem,sizeof(stored));
        assert(stored[0]==9.0f && stored[1]==9.0f && stored[2]==9.0f && stored[3]==9.0f);
        assert(f32(t.vf[3][0])==5.0f && f32(t.vf[3][1])==5.0f &&
               f32(t.vf[3][2])==5.0f && f32(t.vf[3][3])==5.0f);
    }
    {
        uint8_t m[64]={0};
        uint32_t x;
        /* I-bit loads VI21; lower instruction in the same LIW is ignored. */
        x=0x12345678u; memcpy(m+0,&x,4); x=0x80000000u; memcpy(m+4,&x,4);
        x=lower(0x08,0,21,2)|5u; memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(t.vi[21]==0x12345678u && t.vi[2]==0x1234567du);
    }
    {
        uint8_t m[64]={0};
        uint32_t x;
        x=0; memcpy(m+8,&x,4);
        /* DIV Q, vf1.x, vf2.x uses lower special opcode 0x7c. */
        x=lower(0x7c,14,1,2); memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4);
        x=0; memcpy(m+24,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+28,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(6.0f); t.vf[2][0]=u32(2.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.q)==3.0f);
    }
    {
        uint8_t m[80]={0}; uint32_t x;
        /* FCEQ VI01,0x123 and FCGET VI04. Destination VI01 is encoded explicitly. */
        x=lower(0x10,0,0,0)|0x123u; memcpy(m+0,&x,4); x=0; memcpy(m+4,&x,4);
        x=lower(0x1c,0,0,4); memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif)); t.clip_flag=0x123u; assert(t.vf[0][3]==u32(1.0f));
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,16)==0);
        assert(t.vi[1]==1u);
        assert(t.vi[4]==0x123u);

    }
    {
        uint8_t m[64]={0}; uint32_t x;
        /* FMEQ compares MAC flag with VI source; FMAND/FMOR use VI source. */
        x=lower(0x08,0,0,2)|0x55u; memcpy(m+0,&x,4); x=0; memcpy(m+4,&x,4);
        x=lower(0x18,0,2,3); memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
        x=lower(0x1a,0,2,0); memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4);
        x=lower(0x1b,0,2,0); memcpy(m+24,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+28,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[2]=0x55u; t.mac_flag=0x55u; assert(tsfp_vu_execute(m,sizeof(m),0,&t,16)==0);
        assert(t.vi[3]==1u);
    }

    {
        /* Special lower pipeline: LQI/SQI post-increment and field masks. */
        uint8_t m[80]={0}; uint32_t x;
        uint32_t lqi=0x80000000u | (4u<<21) | (2u<<16) | (3u<<11) | 0x37cu;
        uint32_t sqi=0x80000000u | (4u<<21) | (4u<<16) | (2u<<11) | 0x37du;
        float src[4]={7,8,9,10};
        memcpy(mem+64,src,16);
        memcpy(m+0,&lqi,4); x=0; memcpy(m+4,&x,4);
        memcpy(m+8,&sqi,4); x=0; memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[3]=4; t.vi[4]=6;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,16)==0);
        float got[4]; memcpy(got,t.vf[2],16);
        assert(got[0]==0.0f && got[1]==8.0f && got[2]==0.0f && got[3]==0.0f);
        assert(t.vi[3]==5u && t.vi[4]==7u);
        memcpy(got,mem+96,16);
        assert(got[0]==0.0f && got[1]==8.0f && got[2]==0.0f && got[3]==0.0f);
    }
    {
        /* A lower LQI write to VF2 must survive an unrelated upper VF3 write. */
        uint8_t m[64]={0}; uint32_t x;
        uint32_t lqi=0x80000000u | (4u<<21) | (2u<<16) | (3u<<11) | 0x37cu;
        x=lqi; memcpy(m+0,&x,4);
        x=upper(0x28,4,1,2,0xf); memcpy(m+4,&x,4); /* ADD VF4,VF1,VF2 */
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        float src[4]={7,8,9,10}; memcpy(mem+64,src,16);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[3]=4;
        memcpy(t.vf[1],(float[4]){1,1,1,1},16);
        memcpy(t.vf[2],(float[4]){2,2,2,2},16);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.vf[2][0])==2.0f && f32(t.vf[2][1])==8.0f);
        assert(f32(t.vf[4][0])==3.0f && f32(t.vf[4][1])==3.0f);
    }
    {
        /* VF0 is the architectural constant (0,0,0,1); vector loads must not modify it. */
        uint8_t m[32]={0}; uint8_t lmem[64]={0}; uint8_t lgif[64]={0}; uint32_t x;
        uint32_t src[4]={1u,2u,3u,4u}; memcpy(lmem,src,sizeof(src));
        x=0x80000000u | (15u<<21) | (0u<<16) | (0u<<11) | 0x37cu; memcpy(m,&x,4);
        x=0; memcpy(m+4,&x,4); x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,0,0,0,0); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,lmem,sizeof(lmem),lgif,sizeof(lgif));
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(t.vf[0][0]==0u && t.vf[0][1]==0u && t.vf[0][2]==0u && t.vf[0][3]==u32(1.0f));
    }
    {
        /* MTIR/MFIR transfer only the selected field / 16-bit integer value. */
        uint8_t m[80]={0}; uint32_t x;
        uint32_t mtir=0x80000000u | (3u<<21) | (3u<<16) | (12u<<11) | 0x3fcu;
        uint32_t mfir=0x80000000u | (1u<<21) | (5u<<16) | (3u<<11) | 0x3fdu;
        memcpy(m+0,&mtir,4); x=0; memcpy(m+4,&x,4);
        memcpy(m+8,&mfir,4); x=0; memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[12][3]=0x1234abcd; t.vi[3]=0;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,16)==0);
        assert(t.vi[3]==0xabcd);
        assert(t.vf[5][0]==0.0f && t.vf[5][1]==0.0f && t.vf[5][2]==0.0f);
        assert(t.vf[5][3]==0xffffabcdu);
    }
    {
        uint8_t m[32]={0}; uint32_t x=0;
        x=0; memcpy(m,&x,4); x=upper(0x2e,3,1,2,0xe); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        float a[4]={10,20,30,0}, b[4]={2,3,4,0};
        memcpy(t.acc,a,16); memcpy(t.vf[1],(float[4]){1,2,3,0},16); memcpy(t.vf[2],b,16);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        float got[4]; memcpy(got,t.vf[3],16);
        assert(got[0]==2.0f && got[1]==14.0f && got[2]==27.0f);
    }
    {
        /* MR32 rotates source fields x<-y, y<-z, z<-w, w<-x for selected destinations. */
        uint8_t m[32]={0}; uint32_t x;
        x=0x80000000u | (15u<<21) | (6u<<16) | (5u<<11) | 0x33du; memcpy(m,&x,4);
        x=0; memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[5][0]=u32(1.0f); t.vf[5][1]=u32(2.0f); t.vf[5][2]=u32(3.0f); t.vf[5][3]=u32(4.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.vf[6][0])==4.0f && f32(t.vf[6][1])==1.0f &&
               f32(t.vf[6][2])==2.0f && f32(t.vf[6][3])==3.0f);
    }
    {
        /* ILWR/ISWR select one VF word using the same X/Y/Z/W destination mask. */
        uint8_t m[48]={0}; uint32_t x;
        x=0x0000beefu; memcpy(mem+64+4,&x,4);
        x=0x80000000u | (4u<<21) | (5u<<16) | (3u<<11) | 0x3feu; memcpy(m,&x,4);
        x=0x80000000u | (2u<<21) | (5u<<16) | (4u<<11) | 0x3ffu; memcpy(m+8,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[3]=4; t.vi[4]=4;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,16)==0);
        assert(t.vi[5]==0xbeefu);
        assert(rd32(mem+64+8)==0xbeefu);
    }
    {
        /* ILWR multi-bit fields use the VU's wired Y/Z lane-code priority;
           an empty field also aliases W rather than performing no write. */
        uint8_t m[80]={0}; uint32_t x;
        uint32_t w=0x0000beefu; memcpy(mem+64+12,&w,4);
        x=0x80000000u | (6u<<21) | (5u<<16) | (3u<<11) | 0x3feu; memcpy(m,&x,4);
        x=0x80000000u | (0u<<21) | (6u<<16) | (3u<<11) | 0x3feu; memcpy(m+8,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[3]=4;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,16)==0);
        assert(t.vi[5]==0xbeefu && t.vi[6]==0xbeefu);
    }
    {
        /* LQD/SQD decrement the address from VI0 but VI0 itself remains
           hardwired to zero; the pre-decrement address wraps to quadword 1023. */
        uint8_t m[32]={0}; uint8_t lmem[16384]={0}; uint8_t lgif[64]={0}; uint32_t x;
        uint32_t src[4]={0x11u,0x22u,0x33u,0x44u};
        memcpy(lmem+0x3ff0,src,sizeof(src));
        x=0x80000000u | (15u<<21) | (1u<<16) | 0x37eu; memcpy(m,&x,4);
        x=0; memcpy(m+4,&x,4); x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,lmem,sizeof(lmem),lgif,sizeof(lgif));
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(t.vi[0]==0);
        assert(t.vf[1][0]==0x11u && t.vf[1][1]==0x22u &&
               t.vf[1][2]==0x33u && t.vf[1][3]==0x44u);
    }

    {
        /* E-bit terminates after exactly one following LIW (the delay slot). */
        uint8_t m[40]={0}; uint32_t x;
        x=0; memcpy(m+0,&x,4); x=0x40000000u | upper(0x28,3,1,2,0xf); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=upper(0x28,4,1,2,0xf); memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=upper(0x28,5,1,2,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        memcpy(t.vf[1],(float[4]){2,2,2,2},16); memcpy(t.vf[2],(float[4]){3,3,3,3},16);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.vf[3][0])==5.0f);
        assert(f32(t.vf[4][0])==5.0f);
        assert(f32(t.vf[5][0])==0.0f);
        assert(t.pc==2u);
    }

    {
        /* Upper writes to VF3.x while lower LQI writes VF3.y/z.  Hardware
           resolves this at register granularity, so the entire lower write
           is discarded and VF3.y/z keep their pre-cycle values. */
        uint8_t m[32]={0}; uint32_t x;
        uint32_t lqi=0x80000000u | (0xeu<<21) | (3u<<16) | (4u<<11) | 0x37cu;
        x=lqi; memcpy(m+0,&x,4);
        x=upper(0x28,3,1,2,0x8); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        uint32_t src[4]={7u,8u,9u,10u}; memcpy(mem+64,src,sizeof(src));
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[4]=4;
        memcpy(t.vf[1],(float[4]){1,1,1,1},16);
        memcpy(t.vf[2],(float[4]){2,2,2,2},16);
        memcpy(t.vf[3],(float[4]){9,9,9,9},16);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.vf[3][0])==3.0f && f32(t.vf[3][1])==9.0f &&
               f32(t.vf[3][2])==9.0f && f32(t.vf[3][3])==9.0f);
    }

    {
        /* CLIP appends one six-bit result for x/y/z against |VF[ft].w|. */
        uint8_t m[32]={0}; uint32_t x;
        x=0; memcpy(m,&x,4); x=upper(0x3f,7,1,2,0xe); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        memcpy(t.vf[1],(float[4]){3,-3,1,0},16);
        memcpy(t.vf[2],(float[4]){0,0,0,2},16);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(t.clip_flag==0x9u); /* x+ (bit0) + y- (bit3) */
    }

    {
        /* MAC flags use four bits per component: Z/S/U/O. */
        uint8_t m[40]={0}; uint32_t x;
        x=0; memcpy(m,&x,4); x=upper(0x28,3,1,2,0xf); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=0u; t.vf[1][1]=u32(-1.0f); t.vf[1][2]=0x000116c2u; t.vf[1][3]=u32(INFINITY);
        memcpy(t.vf[2],(float[4]){0,0,0,0},16);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert((t.mac_flag & 0x0008u)!=0u); /* Zx */
        assert((t.mac_flag & 0x0040u)!=0u); /* Sy */
        assert((t.mac_flag & 0x0200u)!=0u); /* Uz */
        assert((t.mac_flag & 0x1000u)!=0u); /* Ow */
    }

    {
        /* Status low bits summarize MAC Z/S/U/O; bits 6-9 are sticky. */
        uint8_t m[32]={0}; uint32_t x;
        x=0; memcpy(m,&x,4); x=upper(0x28,3,1,2,0xf); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=0u; t.vf[1][1]=u32(-1.0f); t.vf[1][2]=0x000116c2u; t.vf[1][3]=u32(INFINITY);
        memcpy(t.vf[2],(float[4]){0,0,0,0},16);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert((t.status_flag&0x0fu)==0x0fu);
        assert((t.status_flag&0x3c0u)==0x3c0u);
    }

    {
        /* DIV/RSQRT/SQRT update invalid/divide-by-zero status bits. */
        uint8_t m[96]={0}; uint32_t x;
        x=lower(0x7c,14,1,2); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u|upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4);
        TsFpVuState d; tsfp_vu_state_init(&d,mem,sizeof(mem),gif,sizeof(gif));
        d.vf[1][0]=0.0f; d.vf[2][0]=0.0f;
        assert(tsfp_vu_execute(m,sizeof(m),0,&d,1)==-2);
        assert((d.q_pending_status&(1u<<4))!=0u && (d.q_pending_status&(1u<<10))!=0u);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=0.0f; t.vf[2][0]=0.0f;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert((t.status_flag&(1u<<4))!=0u && (t.status_flag&(1u<<10))!=0u);
        t.status_flag=0;
        t.vf[1][0]=1.0f; t.vf[2][0]=0.0f;
        TsFpVuState d2; tsfp_vu_state_init(&d2,mem,sizeof(mem),gif,sizeof(gif));
        d2.vf[1][0]=1.0f; d2.vf[2][0]=0.0f;
        assert(tsfp_vu_execute(m,sizeof(m),0,&d2,1)==-2);
        assert((d2.q_pending_status&(1u<<5))!=0u && (d2.q_pending_status&(1u<<11))!=0u);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert((t.status_flag&(1u<<5))!=0u && (t.status_flag&(1u<<11))!=0u);
    }

    {
        /* FMAC arithmetic conditions denormals to signed zero and overflow to max. */
        uint8_t m[32]={0}; uint32_t x;
        x=0; memcpy(m,&x,4); x=upper(0x28,3,1,2,0xf); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u | upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=0x000116c2u; t.vf[2][0]=0.0f;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.vf[3][0])==0.0f);
        assert((t.mac_flag&0x808u)==0x808u); /* Ux + Zx */
        t.status_flag=0; t.mac_flag=0;
        t.vf[1][0]=u32(INFINITY); t.vf[2][0]=0u;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.vf[3][0])==FLT_MAX);
        assert((t.mac_flag&0x8000u)==0x8000u); /* Ox */
    }

    {
        /* FMAC flag consumers observe the result four LIWs later. */
        uint8_t m[48]={0}; uint32_t x;
        x=0; memcpy(m,&x,4); x=upper(0x28,3,1,2,0x8); memcpy(m+4,&x,4);
        x=lower(0x1a,3,2,0); memcpy(m+8,&x,4); x=upper(0x3f,11,0,0,0xf); memcpy(m+12,&x,4);
        for(unsigned n=2;n<5;n++){
            x=0; memcpy(m+n*8,&x,4);
            x=upper(0x3f,11,0,0,0xf); memcpy(m+n*8+4,&x,4);
        }
        x=lower(0x1a,0,2,4); memcpy(m+40,&x,4); x=upper(0x3f,11,0,0,0xf); memcpy(m+44,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[2]=0x80u; t.vf[1][0]=u32(-1.0f); t.vf[2][0]=0.0f;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,6)==-2);
        assert(t.vi[3]==0u);
        assert(t.vi[4]==0x80u);
    }

    {
        /* EFU producers have 10-cycle throughput for the 11-cycle ESADD latency. */
        uint8_t m[24]={0}; uint32_t x;
        x=0; memcpy(m,&x,4); x=upper(0x3c,28,1,0,0xf); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=upper(0x3c,28,1,0,0xf); memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u|upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(2.0f); t.vf[1][1]=u32(3.0f); t.vf[1][2]=u32(6.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,2)==-2);
        assert(f32(t.p)==49.0f);
        assert(t.p_pending==1u && t.p_pending_cycles==11u);
    }
    {
        /* Ordinary LIWs advance EFU latency; MFP observes P once writeback has completed. */
        uint8_t m[31u*8u]={0}; uint32_t x;
        x=0; memcpy(m,&x,4); x=upper(0x3c,28,1,0,0xf); memcpy(m+4,&x,4); /* ESADD */
        for(unsigned i=1;i<29;i++){
            x=0; memcpy(m+i*8u,&x,4);
            x=upper(0x3f,11,0,0,0xf); memcpy(m+i*8u+4,&x,4);
        }
        x=0x80000000u|(0xfu<<21)|(3u<<16)|0x67cu; memcpy(m+29u*8u,&x,4); /* MFP */
        x=0; memcpy(m+29u*8u+4,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(2.0f); t.vf[1][1]=u32(3.0f); t.vf[1][2]=u32(6.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,30)==-2);
        assert(t.p_pending==0u && f32(t.p)==49.0f);
        assert(f32(t.vf[3][0])==49.0f);
    }

    {
        /* A second direct FDIV opcode must wait for the shared Q pipeline. */
        uint8_t m[24]={0}; uint32_t x;
        x=lower(0x7c,0,1,2); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
        x=lower(0x7c,0,3,4); memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u|upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(6.0f); t.vf[2][0]=u32(2.0f);
        t.vf[3][0]=u32(9.0f); t.vf[4][0]=u32(3.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,2)==-2);
        assert(f32(t.q_pending_value)==3.0f);
        assert(t.q_pending_cycles==7u);
    }

    {
        /* WAITQ makes a pending DIV result visible before the following upper op. */
        uint8_t m[48]={0}; uint32_t x;
        x=lower(0x7c,14,1,2); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
        /* WAITQ is special1 opcode 0x3b, encoded in the 0x3bf low-word slot. */
        x=0x80000000u | 0x3bfu; memcpy(m+8,&x,4);
        x=upper(0x20,3,1,0,0x8); memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u|upper(0x3f,11,0,0,0xf); memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(6.0f); t.vf[2][0]=u32(2.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.vf[3][0])==9.0f);
    }
    {
        /* EFU/P path: ESIN writes P asynchronously, WAITP publishes it,
           and MFP transfers the synchronized P value to a VF field. */
        uint8_t m[40]={0}; uint32_t x;
        uint32_t esin=(31u<<6) | (1u<<11) | 0x3cu;
        uint32_t waitp=0x80000000u | 0x7bfu;
        uint32_t mfp=0x80000000u | (0xfu<<21) | (3u<<16) | 0x67cu;
        x=0; memcpy(m,&x,4); x=esin; memcpy(m+4,&x,4);
        x=waitp; memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
        x=mfp; memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4);
        x=0; memcpy(m+24,&x,4); x=0x40000000u|upper(0x3f,11,0,0,0xf); memcpy(m+28,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(0.5f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,1)==-2);
        assert(t.p_pending==1u);
        assert(fabsf(f32(t.p_pending_value)-sinf(0.5f))<1e-6f);
        assert(tsfp_vu_execute(m,sizeof(m),1,&t,1)==-2);
        assert(t.p_pending==0u);
        assert(fabsf(f32(t.p)-sinf(0.5f))<1e-6f);
        assert(tsfp_vu_execute(m,sizeof(m),2,&t,1)==-2);
        assert(fabsf(f32(t.vf[3][0])-sinf(0.5f))<1e-6f);
    }

    {
        /* EFU vector semantics: ESADD/ERSADD operate on the squared-length
           sum; EATANxy/xz return zero when the x component is zero. */
        uint8_t m[96]={0}; uint32_t x;
        x=0; memcpy(m,&x,4); x=upper(0x3c,28,1,0,0xf); memcpy(m+4,&x,4); /* ESADD */
        x=0; memcpy(m+8,&x,4); x=upper(0x3d,28,1,0,0xf); memcpy(m+12,&x,4); /* ERSADD */
        x=0; memcpy(m+16,&x,4); x=upper(0x3c,29,1,0,0xf); memcpy(m+20,&x,4); /* EATANxy */
        x=0; memcpy(m+24,&x,4); x=upper(0x3d,29,1,0,0xf); memcpy(m+28,&x,4); /* EATANxz */
        x=0; memcpy(m+32,&x,4); x=0x40000000u|upper(0x3f,11,0,0,0xf); memcpy(m+36,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(0.0f); t.vf[1][1]=u32(3.0f); t.vf[1][2]=u32(6.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,32)==0);
        assert(f32(t.p)==0.0f);
        /* Synchronize a standalone ESADD result through WAITP/MFP. */
        memset(m,0,sizeof(m));
        x=0; memcpy(m,&x,4); x=upper(0x3c,28,1,0,0xf); memcpy(m+4,&x,4);
        x=0x80000000u|0x7bfu; memcpy(m+8,&x,4);
        x=0; memcpy(m+12,&x,4);
        x=0x80000000u|(0xfu<<21)|(3u<<16)|0x67cu; memcpy(m+16,&x,4);
        x=0; memcpy(m+20,&x,4);
        x=0; memcpy(m+24,&x,4); x=0x40000000u|upper(0x3f,11,0,0,0xf); memcpy(m+28,&x,4);
        t.pc=0; t.p_pending=0; t.p=0; t.vf[3][0]=0;
        t.vf[1][0]=u32(2.0f); t.vf[1][1]=u32(3.0f); t.vf[1][2]=u32(6.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,16)==0);
        assert(f32(t.p)==49.0f && f32(t.vf[3][0])==49.0f);
    }
    {
        /* Integer immediates use distinct signed/unsigned widths and VI
           arithmetic wraps in the 16-bit integer register. */
        uint8_t m[48]={0}; uint32_t x;
        x=lower(0x32,0,1,2)|((uint32_t)0x1fu<<6); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4); /* IADDI -1 */
        x=lower(0x08,0,1,3)|0x7ffu; memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4); /* IADDIU +2047 */
        x=lower(0x09,0,1,4)|0x7ffu; memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4); /* ISUBIU -2047 */
        x=lower(0x34,5,1,6); memcpy(m+24,&x,4); x=0; memcpy(m+28,&x,4);
        x=lower(0x35,7,1,6); memcpy(m+32,&x,4); x=0x40000000u; memcpy(m+36,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[1]=0xffffu; t.vi[6]=0x00f0u;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,6)==0);
        assert(t.vi[2]==0xfffeu);
        assert(t.vi[3]==0x07feu);
        assert(t.vi[4]==0xf800u);
        assert(t.vi[5]==0x00f0u && t.vi[7]==0xffffu);
    }
    {
        /* LQ/SQ honor the per-lane destination/source mask. */
        uint8_t m[32]={0}; uint32_t x;
        x=lower(0x00,0,0,2)|(0xau<<21); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u; memcpy(m+12,&x,4);
        uint8_t data[64]={0}; uint32_t vals[4]={u32(1.0f),u32(2.0f),u32(3.0f),u32(4.0f)};
        memcpy(data,vals,16);
        TsFpVuState t; tsfp_vu_state_init(&t,data,sizeof(data),gif,sizeof(gif));
        t.vf[2][0]=u32(9.0f); t.vf[2][1]=u32(9.0f); t.vf[2][2]=u32(9.0f); t.vf[2][3]=u32(9.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,3)==0);
        assert(f32(t.vf[2][0])==1.0f && f32(t.vf[2][1])==9.0f &&
               f32(t.vf[2][2])==3.0f && f32(t.vf[2][3])==9.0f);
        memset(m,0,sizeof(m));
        x=lower(0x01,0,0,2)|(0xau<<21); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u; memcpy(m+12,&x,4);
        uint8_t out[64]={0};
        TsFpVuState q; tsfp_vu_state_init(&q,out,sizeof(out),gif,sizeof(gif));
        q.vf[2][0]=u32(1.0f); q.vf[2][1]=u32(2.0f); q.vf[2][2]=u32(3.0f); q.vf[2][3]=u32(4.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&q,3)==0);
        assert(f32(rd32(out+0))==1.0f && f32(rd32(out+4))==0.0f &&
               f32(rd32(out+8))==3.0f && f32(rd32(out+12))==0.0f);
    }
    {
        /* VU random-unit opcodes: RNEXT=0x40, RGET=0x41, RINIT=0x42, RXOR=0x43. */
        uint8_t m[40]={0}; uint32_t x;
        x=0x8000043eu|(1u<<11); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4); /* RINIT.x */
        x=0x8000043du|(0xau<<21)|(2u<<16); memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4); /* RGET xz */
        x=0x8000043cu|(4u<<21)|(3u<<16); memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4); /* RNEXT y */
        x=0x8000043fu|(1u<<11); memcpy(m+24,&x,4); x=0x40000000u; memcpy(m+28,&x,4); /* RXOR.x + E */
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=0x00123456u;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,5)==0);
        assert(t.r==0x3fb65cfbu);
        assert(t.vf[2][0]==0x3f923456u && t.vf[2][2]==0x3f923456u);
        assert(t.vf[3][1]==0x3fa468adu);
    }
    {
        /* Lower flag operations use the documented 0x10-0x1c dispatch slots. */
        {
            uint8_t m[40]={0}; uint32_t x;
            x=flag_imm(0x1a,5,0); x|=(6u<<11); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
            x=flag_imm(0x18,7,0); x|=(6u<<11); memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
            x=flag_imm(0x1b,8,0); x|=(6u<<11); memcpy(m+16,&x,4); x=0x40000000u; memcpy(m+20,&x,4);
            TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
            t.mac_flag=0x0f0fu; t.vi[6]=0x00ffu;
            assert(tsfp_vu_execute(m,sizeof(m),0,&t,4)==0);
            assert(t.vi[5]==0x000fu && t.vi[7]==0u && t.vi[8]==0x0fffu);
        }
        {
            uint8_t m[40]={0}; uint32_t x;
            x=flag_imm(0x16,2,0x0d0u); memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
            x=flag_imm(0x14,3,0xa5au); memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
            x=flag_imm(0x17,4,5u); memcpy(m+16,&x,4); x=0x40000000u; memcpy(m+20,&x,4);
            TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
            t.status_flag=0xa5au;
            assert(tsfp_vu_execute(m,sizeof(m),0,&t,4)==0);
            assert(t.vi[2]==0x50u && t.vi[3]==1u && t.vi[4]==0xa5fu);
        }
        {
            uint8_t m[48]={0}; uint32_t x;
            x=(0x11u<<25)|0x123456u; memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
            x=(0x12u<<25)|0x0000ffu; memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
            x=(0x10u<<25)|0x123456u; memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4);
            x=(0x13u<<25)|0xedcba9u; memcpy(m+24,&x,4); x=0; memcpy(m+28,&x,4);
            x=flag_imm(0x1c,9,0); memcpy(m+32,&x,4); x=0x40000000u; memcpy(m+36,&x,4);
            TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
            assert(tsfp_vu_execute(m,sizeof(m),0,&t,6)==0);
            assert(t.clip_flag==0x123456u && t.vi[1]==1u && t.vi[9]==0x456u);
        }
        {
            uint8_t m[24]={0}; uint32_t x=flag_imm(0x15,10,0xc40u);
            memcpy(m,&x,4); x=0; memcpy(m+4,&x,4);
            memcpy(m+8,&x,0); x=flag_imm(0x15,10,0xc40u); memcpy(m+8,&x,4); x=0x40000000u; memcpy(m+12,&x,4);
            x=flag_imm(0x15,10,0xc40u); memcpy(m+16,&x,4); x=0; memcpy(m+20,&x,4);
            TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
            t.status_flag=0xa5au;
            assert(tsfp_vu_execute(m,sizeof(m),0,&t,3)==0);
            assert(t.status_flag==0xc5au);
        }
    }
    {
        /* VU branch opcodes use JR/JALR at 0x24/0x25 and conditional
           branches at 0x28/0x29/0x2c-0x2f. Every branch has one delay LIW. */
        const uint8_t cond_ops[]={0x28u,0x29u,0x2cu,0x2du,0x2eu,0x2fu};
        const int expected_taken[]={1,1,1,1,1,1};
        for(unsigned k=0;k<sizeof(cond_ops);k++){
            uint8_t m[40]={0}; uint32_t x;
            x=branch(cond_ops[k],1,2,2); memcpy(m,&x,4);
            x=lower(0x08,0,0,4)|9u; memcpy(m+4,&x,4);
            x=lower(0x08,0,0,5)|7u; memcpy(m+16, &x,4);
            x=lower(0x08,0,0,5)|13u; memcpy(m+24, &x,4); x=0x40000000u; memcpy(m+28,&x,4);
            TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
            t.vi[1]=(cond_ops[k]==0x28u||cond_ops[k]==0x29u)?5u:0xffffffffu;
            t.vi[2]=(cond_ops[k]==0x28u||cond_ops[k]==0x29u)?5u:0u;
            if(cond_ops[k]==0x2du) t.vi[1]=1u;
            if(cond_ops[k]==0x2cu) t.vi[1]=(uint32_t)-1;
            if(cond_ops[k]==0x2eu) t.vi[1]=(uint32_t)-1;
            if(cond_ops[k]==0x2fu) t.vi[1]=1u;
            assert(tsfp_vu_execute(m,sizeof(m),0,&t,4)==0);
            assert(t.vi[4]==9u);
            assert(t.vi[5]==(expected_taken[k]?13u:7u));
        }
        {
            uint8_t m[40]={0}; uint32_t x;
            x=branch(0x24u,1,0,0); memcpy(m,&x,4);
            x=lower(0x08,0,0,4)|9u; memcpy(m+4,&x,4);
            x=lower(0x08,0,0,5)|7u; memcpy(m+16,&x,4);
            x=lower(0x08,0,0,5)|13u; memcpy(m+24,&x,4); x=0x40000000u; memcpy(m+28,&x,4);
            TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
            t.vi[1]=3u;
            assert(tsfp_vu_execute(m,sizeof(m),0,&t,4)==0);
            assert(t.vi[4]==9u && t.vi[5]==13u);
        }
        {
            uint8_t m[40]={0}; uint32_t x;
            x=branch(0x25u,1,15,0); memcpy(m,&x,4);
            x=lower(0x08,0,0,4)|9u; memcpy(m+4,&x,4);
            x=lower(0x08,0,0,5)|7u; memcpy(m+16,&x,4);
            x=lower(0x08,0,0,5)|13u; memcpy(m+24,&x,4);
            TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
            t.vi[1]=3u;
            assert(tsfp_vu_execute(m,sizeof(m),0,&t,4)==0);
            assert(t.vi[4]==9u && t.vi[5]==13u && t.vi[15]==2u);
        }
        {
            uint8_t m[40]={0}; uint32_t x;
            x=branch(0x21u,0,0,2); memcpy(m,&x,4);
            x=lower(0x08,0,0,4)|9u; memcpy(m+4,&x,4);
            x=lower(0x08,0,0,5)|7u; memcpy(m+16,&x,4);
            x=lower(0x08,0,0,5)|13u; memcpy(m+24,&x,4);
            TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
            assert(tsfp_vu_execute(m,sizeof(m),0,&t,4)==0);
            assert(t.vi[4]==9u && t.vi[5]==13u && t.vi[15]==2u);
        }
    }
    {
        /* Branch comparisons consume the low 16 bits of VI registers. */
        uint8_t m[40]={0}; uint32_t x;
        x=branch(0x2cu,1,0,2); memcpy(m,&x,4);
        x=0; memcpy(m+4,&x,4);
        x=lower(0x08,0,0,5)|7u; memcpy(m+16,&x,4);
        x=lower(0x08,0,0,5)|13u; memcpy(m+24,&x,4); x=0x40000000u; memcpy(m+28,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[1]=0xffffu;
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,5)==0 && t.vi[5]==13u);

        memset(m,0,sizeof(m));
        x=branch(0x28u,1,2,2); memcpy(m,&x,4);
        x=0; memcpy(m+4,&x,4);
        x=lower(0x08,0,0,5)|7u; memcpy(m+16,&x,4);
        x=lower(0x08,0,0,5)|13u; memcpy(m+24,&x,4); x=0x40000000u; memcpy(m+28,&x,4);
        TsFpVuState e; tsfp_vu_state_init(&e,mem,sizeof(mem),gif,sizeof(gif));
        e.vi[1]=0x00010001u; e.vi[2]=1u;
        assert(tsfp_vu_execute(m,sizeof(m),0,&e,5)==0 && e.vi[5]==13u);
    }
    return 0;
}
