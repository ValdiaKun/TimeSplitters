#include "fp_vu.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t v(uint8_t cmd, uint8_t num, uint16_t imm) {
    return ((uint32_t)cmd << 24) | ((uint32_t)num << 16) | imm;
}

int main(void) {
    uint8_t micro[32] = {0};
    uint32_t lower0 = v(0x00,0,0);
    uint32_t upper0 = 0;
    uint32_t lower1 = (0x6cu << 25) | (3u << 11);
    uint32_t upper1 = 0;
    uint32_t lower2 = 0;
    uint32_t upper2 = 0x40000000u;
    memcpy(micro + 0, &lower0, 4); memcpy(micro + 4, &upper0, 4);
    memcpy(micro + 8, &lower1, 4); memcpy(micro + 12, &upper1, 4);
    memcpy(micro + 16, &lower2, 4); memcpy(micro + 20, &upper2, 4);
    uint32_t pc = UINT32_MAX;
    assert(tsfp_vu_probe(micro, sizeof(micro), 0, &pc) == 0);
    assert(pc == 1);

    {
        /* The probe must inspect the one-instruction E-bit delay slot. */
        uint8_t delayed[24]={0}; uint32_t lo=0,up=0;
        uint32_t e=0x40000000u;
        uint32_t xg=(0x6cu<<25)|(3u<<11);
        memcpy(delayed+0,&lo,4); memcpy(delayed+4,&up,4);
        memcpy(delayed+8,&lo,4); memcpy(delayed+12,&e,4);
        memcpy(delayed+16,&xg,4); memcpy(delayed+20,&up,4);
        pc=UINT32_MAX;
        assert(tsfp_vu_probe(delayed,sizeof(delayed),0,&pc)==0);
        assert(pc==2u);
    }
    {
        uint8_t elf[0x120];
        memset(elf,0,sizeof(elf));
        elf[0]=0x7f; elf[1]='E'; elf[2]='L'; elf[3]='F'; elf[4]=1; elf[5]=1;
        elf[32]=0x40; elf[46]=40; elf[48]=3; elf[50]=1;
        const char names[]="\0.shstrtab\0.vutext\0";
        memcpy(elf+0xc0,names,sizeof(names));
        uint32_t sh1_name=1, sh1_off=0xc0, sh1_len=(uint32_t)sizeof(names);
        uint32_t sh2_name=11, sh2_off=0x100, sh2_len=16;
        memcpy(elf+0x40+40+0,&sh1_name,4);
        memcpy(elf+0x40+40+16,&sh1_off,4);
        memcpy(elf+0x40+40+20,&sh1_len,4);
        memcpy(elf+0x40+80+0,&sh2_name,4);
        memcpy(elf+0x40+80+16,&sh2_off,4);
        memcpy(elf+0x40+80+20,&sh2_len,4);
        size_t off=0,len=0;
        assert(tsfp_vu_find_vutext(elf,sizeof(elf),&off,&len)==0);
        assert(off==0x100 && len==16);

        /* ELF32 section headers must be large enough for all fields read below. */
        uint16_t short_size=4u, full_size=40u;
        memcpy(elf+46,&short_size,2);
        assert(tsfp_vu_find_vutext(elf,sizeof(elf),&off,&len)==-4);
        memcpy(elf+46,&full_size,2);

        /* Malformed ELF offsets must be rejected without wrapping size_t. */
        uint32_t bad=0xfffffff0u, good=0x40u, overlapping=0u;
        memcpy(elf+32,&overlapping,4);
        assert(tsfp_vu_find_vutext(elf,sizeof(elf),&off,&len)==-5);
        memcpy(elf+32,&bad,4);
        assert(tsfp_vu_find_vutext(elf,sizeof(elf),&off,&len)==-5);
        memcpy(elf+32,&good,4);
        memcpy(elf+0x40+40+16,&bad,4);
        assert(tsfp_vu_find_vutext(elf,sizeof(elf),&off,&len)==-6);
        memcpy(elf+0x40+40+16,&sh1_off,4);
        memcpy(elf+0x40+80+16,&bad,4);
        assert(tsfp_vu_find_vutext(elf,sizeof(elf),&off,&len)==-7);
    }
    return 0;
}
