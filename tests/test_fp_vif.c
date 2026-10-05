#include "fp_vif.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t v(uint8_t cmd, uint8_t num, uint16_t imm) {
    return ((uint32_t)cmd << 24) | ((uint32_t)num << 16) | imm;
}

int main(void) {
    uint8_t d[64] = {0};
    TsFpVifSummary s;
    uint32_t words[9] = {
        v(0x01, 0, 0x0404),
        v(0x6c, 1, 0x8000),
        0x0000802a,
        v(0x30, 0, 0x4000),
        0x00000412, 0, 0x6d2a8008, 0x00d0ffd2,
        0x0000007f
    };
    memcpy(d, words, sizeof(words));
    assert(tsfp_vif_probe(d, sizeof(d), &s) == 0);
    assert(s.stcycl.immediate == 0x0404);
    assert(s.unpack.command == 0x6c && s.unpack.num == 1);
    assert(s.strow.command == 0x30);
    assert(s.row[0] == 0x00000412 && s.row[3] == 0x00d0ffd2);
    assert(s.payload_offset == 32 && s.payload_bytes == 4);
    return 0;
}
