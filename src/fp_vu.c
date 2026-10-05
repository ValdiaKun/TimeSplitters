#include "fp_vu.h"

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
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
