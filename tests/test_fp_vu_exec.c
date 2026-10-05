#include "fp_vu.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
static uint32_t u32(float x){uint32_t v;memcpy(&v,&x,4);return v;}
static float f32(uint32_t x){float v;memcpy(&v,&x,4);return v;}

static uint32_t upper(uint8_t op, uint8_t fd, uint8_t fs, uint8_t ft, uint8_t mask) {
    return ((uint32_t)op) | ((uint32_t)fd<<6) | ((uint32_t)fs<<11) |
           ((uint32_t)ft<<16) | ((uint32_t)mask<<21);
}
static uint32_t lower(uint8_t op, uint8_t fd, uint8_t fs, uint8_t ft) {
    return ((uint32_t)op<<25) | ((uint32_t)fd<<6) | ((uint32_t)fs<<11) | ((uint32_t)ft<<16);
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
        uint8_t m[64]={0};
        uint32_t x;
        /* I-bit loads VI21; lower instruction in the same LIW is ignored. */
        x=0x12345678u; memcpy(m+0,&x,4); x=0x80000000u; memcpy(m+4,&x,4);
        x=lower(0x08,0,21,2)|5u; memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u; memcpy(m+20,&x,4);
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
        x=0; memcpy(m+24,&x,4); x=0x40000000u; memcpy(m+28,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vf[1][0]=u32(6.0f); t.vf[2][0]=u32(2.0f);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(f32(t.q)==3.0f);
    }
    {
        uint8_t m[80]={0}; uint32_t x;
        /* FCEQ VI01,0x123 and FCGET VI04. */
        x=lower(0x10,0,0,0)|0x123u; memcpy(m+0,&x,4); x=0; memcpy(m+4,&x,4);
        x=lower(0x1c,0,0,4); memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u; memcpy(m+20,&x,4);
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
        x=lower(0x1b,0,2,0); memcpy(m+24,&x,4); x=0x40000000u; memcpy(m+28,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        t.vi[2]=0x55u; t.mac_flag=0x55u; assert(tsfp_vu_execute(m,sizeof(m),0,&t,16)==0);
        assert(t.vi[3]==1u);
    }
    {
        uint8_t m[32]={0}; uint32_t x=0;
        x=0; memcpy(m,&x,4); x=upper(0x2e,3,1,2,0x7); memcpy(m+4,&x,4);
        x=0; memcpy(m+8,&x,4); x=0x40000000u; memcpy(m+12,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        float a[4]={10,20,30,0}, b[4]={2,3,4,0};
        memcpy(t.acc,a,16); memcpy(t.vf[1],(float[4]){1,2,3,0},16); memcpy(t.vf[2],b,16);
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        float got[4]; memcpy(got,t.vf[3],16);
        assert(got[0]==2.0f && got[1]==14.0f && got[2]==27.0f);
    }
    return 0;
}
