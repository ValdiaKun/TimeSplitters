#include "fp_vu.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

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
    /* vf1 = (1,2,3,4), vf2 = (5,6,7,8), vf3 = vf1 + vf2 */
    float a[4]={1,2,3,4}, b[4]={5,6,7,8};
    memcpy(mem+32,a,16); memcpy(mem+48,b,16);
    w=lower(0,0,0,1)|2; memcpy(micro+24,&w,4); w=0; memcpy(micro+28,&w,4);
    w=lower(0,0,0,2)|3; memcpy(micro+32,&w,4); w=0; memcpy(micro+36,&w,4);
    w=upper(0x28,3,1,2,0xf); memcpy(micro+40,&w,4); w=0; memcpy(micro+44,&w,4);
    w=0x40000000u; memcpy(micro+52,&w,4); w=0; memcpy(micro+48,&w,4);
    TsFpVuState s; tsfp_vu_state_init(&s,mem,sizeof(mem),gif,sizeof(gif));
    assert(tsfp_vu_execute(micro,sizeof(micro),0,&s,32)==0);
    assert(s.vi[3]==5u);
    float got[4]; memcpy(got,s.vf[3],sizeof(got));
    assert(got[0]==6.0f && got[1]==8.0f && got[2]==10.0f && got[3]==12.0f);
    {
        uint8_t m[64]={0};
        uint32_t x;
        /* I-bit loads VI21; lower instruction in the same LIW is ignored. */
        x=0x12345678u; memcpy(m+0,&x,4); x=0x80000000u; memcpy(m+4,&x,4);
        x=lower(0x08,2,21,0)|5u; memcpy(m+8,&x,4); x=0; memcpy(m+12,&x,4);
        x=0; memcpy(m+16,&x,4); x=0x40000000u; memcpy(m+20,&x,4);
        TsFpVuState t; tsfp_vu_state_init(&t,mem,sizeof(mem),gif,sizeof(gif));
        assert(tsfp_vu_execute(m,sizeof(m),0,&t,8)==0);
        assert(t.vi[21]==0x12345678u && t.vi[2]==0x1234567du);
    }
    return 0;
}
