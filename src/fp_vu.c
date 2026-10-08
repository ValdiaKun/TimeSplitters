#include "fp_vu.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int tsfp_vu_find_vutext(const uint8_t *elf, size_t size, size_t *offset, size_t *length) {
    if (!elf || !offset || !length || size < 52) return -1;
    if (elf[0] != 0x7f || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F') return -2;
    if (elf[4] != 1 || elf[5] != 1) return -3;
    uint32_t shoff = rd32(elf + 32);
    uint16_t shentsz = (uint16_t)elf[46] | ((uint16_t)elf[47] << 8);
    uint16_t shnum = (uint16_t)elf[48] | ((uint16_t)elf[49] << 8);
    uint16_t shstr = (uint16_t)elf[50] | ((uint16_t)elf[51] << 8);
    if (!shentsz || shnum == 0 || shstr >= shnum) return -4;
    size_t table_size=(size_t)shentsz*(size_t)shnum;
    if ((size_t)shoff>size || table_size>size-(size_t)shoff) return -5;
    const uint8_t *strsec = elf + (size_t)shoff + (size_t)shstr * shentsz;
    uint32_t str_off = rd32(strsec + 16), str_len = rd32(strsec + 20);
    if ((size_t)str_off>size || (size_t)str_len>size-(size_t)str_off) return -6;
    for (uint16_t i=0; i<shnum; ++i) {
        const uint8_t *sec = elf + (size_t)shoff + (size_t)i * shentsz;
        uint32_t name = rd32(sec), off = rd32(sec + 16), len = rd32(sec + 20);
        if (name >= str_len || (size_t)off>size || (size_t)len>size-(size_t)off) continue;
        if (str_len - name >= 8u &&
            memcmp(elf + (size_t)str_off + name, ".vutext", 8u) == 0) {
            *offset = off;
            *length = len;
            return 0;
        }
    }
    return -7;
}

int tsfp_vu_probe(const uint8_t *micro, size_t size, uint32_t start, uint32_t *xgkick_pc) {
    if (!micro || !xgkick_pc || (size & 7u) != 0) return -1;
    size_t words = size / 8u;
    if (start >= words) return -2;
    *xgkick_pc = UINT32_MAX;
    int end_pending = 0;
    for (size_t pc = start; pc < words; ++pc) {
        uint32_t lower = rd32(micro + pc * 8u);
        uint32_t upper = rd32(micro + pc * 8u + 4u);
        if (!(upper & 0x80000000u) && ((lower >> 25) & 0x7fu) == 0x6cu) {
            *xgkick_pc = (uint32_t)pc;
        }
        /* E has a one-LIW delay slot; inspect that slot before stopping. */
        if (end_pending) return 0;
        if (upper & 0x40000000u) end_pending = 1;
    }
    return 0;
}
