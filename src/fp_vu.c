#include "fp_vu.h"

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
    if ((size_t)shoff + (size_t)shentsz * shnum > size) return -5;
    const uint8_t *strsec = elf + shoff + (size_t)shstr * shentsz;
    uint32_t str_off = rd32(strsec + 16), str_len = rd32(strsec + 20);
    if ((size_t)str_off + str_len > size) return -6;
    for (uint16_t i=0; i<shnum; ++i) {
        const uint8_t *sec = elf + shoff + (size_t)i * shentsz;
        uint32_t name = rd32(sec), off = rd32(sec + 16), len = rd32(sec + 20);
        if (name >= str_len || (size_t)off + len > size) continue;
        const char *n = (const char *)(elf + str_off + name);
        if (strcmp(n, ".vutext") == 0) {
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
    for (size_t pc = start; pc < words; ++pc) {
        uint32_t lower = rd32(micro + pc * 8u);
        uint32_t upper = rd32(micro + pc * 8u + 4u);
        if (((lower >> 25) & 0x7fu) == 0x6cu) {
            *xgkick_pc = (uint32_t)pc;
        }
        if (upper & 0x40000000u) return 0;
    }
    return 0;
}
