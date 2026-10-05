#include "fp_vif.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t v(uint8_t cmd, uint8_t num, uint16_t imm) {
    return ((uint32_t)cmd << 24) | ((uint32_t)num << 16) | imm;
}

int main(void) {
    uint8_t d[128] = {0};
    TsFpVifSummary s;
    uint32_t words[26] = {
        v(0x01, 0, 0x0404),
        v(0x6c, 1, 0x8000),
        0x0000802a, 0x302e4000, 0x00000412, 0x00000000,
        v(0x6d, 2, 0x8008),
        0x00010044, 0x00caffc9, 0x0001004f, 0x00d0ffb5,
        v(0x14, 0, 0x0683),
        v(0x01, 0, 0x0404),
        v(0x6e, 2, 0x8060),
        0x7f3c6eff, 0x7f3c6eff, 0x7f3f6ae9, 0x7f3f62d2,
        v(0x75, 2, 0x8034),
        0xffe4fecd, 0xff76fecd, 0xffe4fefc, 0xff76fefc,
        v(0x64, 1, 0xc007), 0x00000000, 0x00000410,
        v(0x14, 0, 0x0683),
        v(0x00, 0, 0),
        v(0x00, 0, 0),
        v(0x00, 0, 0)
    };
    memcpy(d, words, sizeof(words));

    assert(tsfp_vif_probe(d, sizeof(d), &s) == 0);
    assert(s.stcycl.immediate == 0x0404);
    assert(s.unpack.command == 0x6c && s.unpack.num == 1);
    assert(s.strow.command == 0x30);
    assert(s.payload_offset == 32 && s.payload_bytes == 16);
    assert(s.cycle_length == 4 && s.write_length == 4);
    assert(s.unpack_address == 0 && s.unpack_top_relative == 1);

    memset(&s, 0, sizeof(s));
    assert(tsfp_vif_scan(d, sizeof(words), &s) == 0);
    assert(s.command_count >= 10);
    assert(s.unpack_count == 5);
    assert(s.mscal_count == 2);
    assert(s.unpack_qwords == 1 + 2 + 2 + 2 + 1);
    return 0;
}
