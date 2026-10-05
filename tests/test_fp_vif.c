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
    uint8_t vu[4096];
    TsFpVifMemorySummary ms;
    uint32_t words[26] = {
        v(0x01, 0, 0x0404),
        v(0x6c, 1, 0x8000),
        0x0000802a, 0x302e4000, 0x00000412, 0x00000000,
        v(0x6d, 2, 0x8008),
        0x00010044, 0x00caffc9, 0x0001004f, 0x00d0ffb5,
        v(0x14, 0, 0x0683),
        v(0x01, 0, 0x0404),
        v(0x6e, 2, 0x8060),
        0x7f3c6eff, 0x7f3c6eff,
        v(0x75, 2, 0x8034),
        0xffe4fecd, 0xff76fecd,
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
    {
        uint8_t z[64] = {0};
        uint32_t h[] = {
            v(0x01,0,0x0000), v(0x6c,1,0x0000),
            0, v(0x30,0,0), 0,0,0,0,
            0,0,0,0
        };
        memcpy(z,h,sizeof(h));
        memset(&s,0,sizeof(s));
        assert(tsfp_vif_probe(z,sizeof(z),&s)==0);
        assert(s.cycle_length==256 && s.write_length==256);
    }

    memset(&s, 0, sizeof(s));
    assert(tsfp_vif_scan(d, sizeof(words), &s) == 0);
    assert(s.command_count >= 10);
    assert(s.unpack_count == 5);
    assert(s.mscal_count == 2);
    assert(s.mscal_address == 0x683);
    assert(s.unpack_qwords == 1 + 2 + 2 + 2 + 1);
    assert(tsfp_vif_unpack_memory(d, sizeof(words), vu, sizeof(vu), &ms) == 0);
    assert(ms.unpack_commands == 5 && ms.mscal_address == 0x683);
    assert(ms.qwords_written == 8);

    {
        uint8_t m[128] = {0};
        size_t p = 0;
        uint32_t row[4] = {10,20,30,40};
        uint32_t col[4] = {100,200,300,400};
        uint32_t payload[4] = {1,2,3,4};
        uint32_t mask = 0x00000038u; /* x=data, y=col, z=protected, w=data */
        uint32_t head[] = {
            v(0x01,0,0x0404), v(0x30,0,0), row[0],row[1],row[2],row[3],
            v(0x31,0,0), col[0],col[1],col[2],col[3],
            v(0x20,0,0), mask,
            v(0x05,0,1), v(0x7c,1,0)
        };
        memcpy(m+p,head,sizeof(head)); p+=sizeof(head);
        memcpy(m+p,payload,sizeof(payload)); p+=sizeof(payload);
        memset(&ms,0,sizeof(ms));
        assert(tsfp_vif_unpack_memory(m,p,vu,sizeof(vu),&ms)==0);
        assert(ms.unpack_commands==1 && ms.qwords_written==1);
        uint32_t got[4];
        memcpy(got, vu, sizeof(got));
        assert(got[0] == 11u);
        assert(got[1] == 100u);
        assert(got[2] == 0u);
        assert(got[3] == 44u);
    }
    {
        uint8_t m[64]={0}; uint32_t head[]={v(0x01,0,0x0101),v(0x60,1,0x0000),0x11223344};
        memcpy(m,head,sizeof(head)); memset(&ms,0,sizeof(ms));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        uint32_t got[4]; memcpy(got,vu,sizeof(got));
        assert(got[0]==0x11223344u && got[1]==0x11223344u && got[2]==0x11223344u && got[3]==0x11223344u);
    }
    {
        uint8_t m[64]={0}; uint32_t head[]={v(0x01,0,0x0101),v(0x64,1,0x0010),0x00000011,0x00000022};
        memcpy(m,head,sizeof(head)); memset(&ms,0,sizeof(ms));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        uint32_t got[4]; memcpy(got,vu+0x100,sizeof(got));
        assert(got[0]==0x11u && got[1]==0x22u && got[2]==0x11u && got[3]==0x22u);
    }
    return 0;
}
