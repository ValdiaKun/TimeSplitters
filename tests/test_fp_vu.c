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
    return 0;
}
