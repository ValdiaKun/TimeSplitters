#include "fp_vif.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t v(uint8_t cmd, uint8_t num, uint16_t imm) {
    return ((uint32_t)cmd << 24) | ((uint32_t)num << 16) | imm;
}
static int mscal_count_cb(uint16_t address,uint8_t *vu,size_t size,
                           uint32_t top,uint32_t itop,void *user) {
    (void)vu; (void)size; (void)top; (void)itop;
    assert(address==0x683u);
    (*(unsigned*)user)++;
    return 0;
}
typedef struct {
    unsigned count;
    uint32_t top[4];
    uint32_t itop[4];
} MscalState;
static int mscnt_cb(uint16_t address,uint8_t *vu,size_t size,
                    uint32_t top,uint32_t itop,void *user) {
    unsigned *count=(unsigned*)user;
    (void)vu; (void)size; (void)top; (void)itop;
    if(*count==0) assert(address==0x683u);
    else assert(address==0xffffu);
    (*count)++;
    return 0;
}
static int mscal_state_cb(uint16_t address,uint8_t *vu,size_t size,
                          uint32_t top,uint32_t itop,void *user) {
    MscalState *s=(MscalState*)user;
    (void)vu; (void)size;
    assert(address==0x683u);
    assert(s->count<4);
    s->top[s->count]=top;
    s->itop[s->count]=itop;
    s->count++;
    return 0;
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
        unsigned callbacks=0;
        memset(&ms,0,sizeof(ms));
        memset(vu,0,sizeof(vu));
        assert(tsfp_vif_unpack_memory_ex(d,sizeof(words),vu,sizeof(vu),&ms,mscal_count_cb,&callbacks)==0);
        assert(callbacks==2 && ms.mscal_address==0x683);
    }

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
        /* V3-16 consumes two 32-bit words per vector.  The second vector
           must begin at the next word boundary, not after 6 packed bytes. */
        uint8_t m[96]={0};
        uint32_t head[]={v(0x01,0,0x0101),v(0x69,2,0x4020),
                         0x11223344,0x00005566,0xaabbccdd,0x0000eeff};
        memcpy(m,head,sizeof(head)); memset(&ms,0,sizeof(ms));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        uint32_t got0[4],got1[4];
        memcpy(got0,vu+0x200,sizeof(got0)); memcpy(got1,vu+0x210,sizeof(got1));
        assert(got0[0]==0x3344u && got0[1]==0x1122u && got0[2]==0x5566u && got0[3]==0u);
        assert(got1[0]==0xccddu && got1[1]==0xaabbu && got1[2]==0xeeffu && got1[3]==0u);
        assert(ms.qwords_written==2);
    }
    {
        uint8_t m[64]={0}; uint32_t head[]={v(0x01,0,0x0101),v(0x64,1,0x0010),0x00000011,0x00000022};
        memcpy(m,head,sizeof(head)); memset(&ms,0,sizeof(ms));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        uint32_t got[4]; memcpy(got,vu+0x100,sizeof(got));
        assert(got[0]==0x11u && got[1]==0x22u && got[2]==0x11u && got[3]==0x22u);
    }
    {
        /* WL > CL: only CL source vectors are consumed per cycle; the remaining
           WL slots repeat the last source vector.  NUM still counts outputs. */
        uint8_t m[128]={0};
        uint32_t head[]={v(0x01,0,0x0201),v(0x6c,4,0x0000),
                         0x11111111,0x11111111,0x11111111,0x11111111,
                         0x22222222,0x22222222,0x22222222,0x22222222};
        memcpy(m,head,sizeof(head));
        memset(&ms,0,sizeof(ms));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        uint32_t got[16];
        memcpy(got,vu,sizeof(got));
        assert(got[0]==0x11111111u && got[4]==0x11111111u);
        assert(got[8]==0x22222222u && got[12]==0x22222222u);
        assert(ms.qwords_written==4);

        /* Scanner and executor must agree on payload consumption when WL > CL. */
        TsFpVifSummary scan;
        memset(&scan,0,sizeof(scan));
        assert(tsfp_vif_scan(m,sizeof(head),&scan)==0);
        assert(scan.unpack_count==1 && scan.unpack_qwords==4);
        assert(scan.unpack_data_bytes==2u*16u);
    }
    {
        /* WL > CL fill state survives an UNPACK boundary.  The first command
           leaves the cycle on its final fill slot; the next command must
           repeat the previous source vector rather than requiring new data. */
        uint8_t m[96]={0};
        uint32_t head[]={
            v(0x01,0,0x0402),
            v(0x6c,3,0x0000),
            0x11111111,0x11111111,0x11111111,0x11111111,
            0x22222222,0x22222222,0x22222222,0x22222222,
            v(0x6c,1,0x0010)
        };
        memcpy(m,head,sizeof(head));
        memset(&ms,0,sizeof(ms)); memset(vu,0,sizeof(vu));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        uint32_t got[16];
        memcpy(got,vu,sizeof(got));
        assert(got[0]==0x11111111u && got[4]==0x22222222u);
        assert(got[8]==0x22222222u);
        uint32_t repeated[4];
        memcpy(repeated,vu+0x100,sizeof(repeated));
        assert(repeated[0]==0x22222222u && repeated[1]==0x22222222u &&
               repeated[2]==0x22222222u && repeated[3]==0x22222222u);
        assert(ms.qwords_written==4);

        TsFpVifSummary scan;
        memset(&scan,0,sizeof(scan));
        assert(tsfp_vif_scan(m,sizeof(head),&scan)==0);
        assert(scan.unpack_count==2 && scan.unpack_qwords==4);
        assert(scan.unpack_data_bytes==2u*16u);
    }
    {
        /* MSCNT resumes through the VU continuation callback instead of being dropped. */
        uint32_t words[]={v(0x14,0,0x0683),v(0x17,0,0)};
        unsigned count=0;
        memset(&ms,0,sizeof(ms)); memset(vu,0,sizeof(vu));
        assert(tsfp_vif_unpack_memory_ex((const uint8_t*)words,sizeof(words),vu,sizeof(vu),
                                         &ms,mscnt_cb,&count)==0);
        assert(count==2 && ms.mscal_address==0x683u);
    }
    {
        /*
         * VIF1 double buffering: MSCAL exposes the current TOPS as TOP,
         * then toggles TOPS between BASE and BASE+OFFSET.  TOP-relative
         * UNPACKs after the first MSCAL must therefore land in the other
         * buffer, and the VU must see ITOP as well.
         */
        uint8_t m[128]={0}; uint32_t head[]={
            v(0x02,0,0x0004), v(0x04,0,0x0007),
            v(0x14,0,0x0683),
            v(0x60,1,0x8000), 0xdeadbeefu,
            v(0x14,0,0x0683)
        };
        memcpy(m,head,sizeof(head));
        MscalState st={0};
        memset(&ms,0,sizeof(ms)); memset(vu,0,sizeof(vu));
        assert(tsfp_vif_unpack_memory_ex(m,sizeof(head),vu,sizeof(vu),&ms,mscal_state_cb,&st)==0);
        assert(st.count==2);
        assert(st.top[0]==0u && st.itop[0]==7u);
        assert(st.top[1]==4u && st.itop[1]==7u);
        uint32_t got=0; memcpy(&got,vu+0x40,4);
        assert(got==0xdeadbeefu);
    }
    {
        /* STMOD/STMASK state must not corrupt an ordinary unmasked UNPACK. */
        uint8_t m[128]={0};
        uint32_t head[]={
            v(0x01,0,0x0808),
            v(0x30,0,0),100,200,300,400,
            v(0x20,0,0),0x00000000u,
            v(0x6c,2,0x0000),
            1,2,3,4,5,6,7,8
        };
        memcpy(m,head,sizeof(head));
        memset(&ms,0,sizeof(ms)); memset(vu,0,sizeof(vu));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        {
            uint32_t got[8];
            memcpy(got,vu,sizeof(got));
            assert(got[0]==1u && got[1]==2u && got[2]==3u && got[3]==4u);
            assert(got[4]==5u && got[5]==6u && got[6]==7u && got[7]==8u);
        }
    }
    {
        /* VIF masks have four groups; cycle slots 4+ use the final group. */
        uint8_t m[192]={0};
        uint32_t head[]={
            v(0x01,0,0x0808),
            v(0x30,0,0),100,200,300,400,
            v(0x31,0,0),500,600,700,800,
            v(0x20,0,0),0x000000c9u,
            v(0x70,5,0x0000),
            1,2,3,4,5
        };
        memcpy(m,head,sizeof(head));
        memset(&ms,0,sizeof(ms)); memset(vu,0,sizeof(vu));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        uint32_t got[20]; memcpy(got,vu,sizeof(got));
        assert(got[0]==100u && got[1]==500u && got[2]==1u && got[3]==0u);
        assert(got[16]==5u && got[17]==5u && got[18]==5u && got[19]==5u);
        assert(ms.qwords_written==5);
    }
    {
        /* STCYCL changes between UNPACKs must affect subsequent payload sizing. */
        uint8_t m[160]={0};
        uint32_t head[]={
            v(0x01,0,0x0101), v(0x6c,4,0x0000),
            1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4,4,
            v(0x01,0,0x0201), v(0x6c,4,0x0010),
            5,5,5,5, 6,6,6,6
        };
        memcpy(m,head,sizeof(head));
        TsFpVifSummary scan;
        memset(&scan,0,sizeof(scan));
        assert(tsfp_vif_scan(m,sizeof(head),&scan)==0);
        assert(scan.unpack_count==2 && scan.unpack_qwords==8);
        assert(scan.unpack_data_bytes==6u*16u);
    }
    {
        /* STCYCL slot position carries across UNPACK commands.  With CL=4/WL=2,
           the first three outputs consume slots 0,1 and skip 2; the next
           command starts at slot 3, skips it, then consumes slot 0. */
        uint8_t m[96]={0};
        uint32_t head[]={
            v(0x01,0,0x0204),
            v(0x6c,3,0x0000),
            1,1,1,1, 2,2,2,2,
            v(0x6c,2,0x0010),
            3,3,3,3
        };
        memcpy(m,head,sizeof(head));
        TsFpVifSummary scan;
        memset(&scan,0,sizeof(scan));
        assert(tsfp_vif_scan(m,sizeof(head),&scan)==0);
        assert(scan.unpack_count==2 && scan.unpack_qwords==5);
        assert(scan.unpack_data_bytes==3u*16u);
    }
    {
        /* The same persistent-slot rule applies when WL > CL. */
        uint8_t m[96]={0};
        uint32_t head[]={
            v(0x01,0,0x0402),
            v(0x6c,3,0x0000),
            1,1,1,1, 2,2,2,2,
            v(0x6c,2,0x0010),
            3,3,3,3
        };
        memcpy(m,head,sizeof(head));
        TsFpVifSummary scan;
        memset(&scan,0,sizeof(scan));
        assert(tsfp_vif_scan(m,sizeof(head),&scan)==0);
        assert(scan.unpack_count==2 && scan.unpack_qwords==5);
        assert(scan.unpack_data_bytes==3u*16u);
    }
    {
        /* V4-5 is packed into one full 32-bit word even though it carries 20 bits.
           A truncated three-byte payload must be rejected before rd32 reads past it. */
        uint8_t m[32]={0};
        uint32_t head[]={v(0x01,0,0x0101),v(0x6f,1,0x0000)};
        memcpy(m,head,sizeof(head));
        m[8]=0x12; m[9]=0x34; m[10]=0x56;
        memset(&ms,0,sizeof(ms));
        assert(tsfp_vif_unpack_memory(m,11,vu,sizeof(vu),&ms)==-5);
    }
    {
        /* TOPS + ADDR wraps at the 1024-qword VU1 address boundary. */
        uint8_t m[32]={0};
        uint32_t head[]={v(0x02,0,0x03ff),v(0x14,0,0x0683),
                         v(0x60,1,0x8001),0x12345678u};
        memcpy(m,head,sizeof(head));
        memset(&ms,0,sizeof(ms));memset(vu,0,sizeof(vu));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),vu,sizeof(vu),&ms)==0);
        uint32_t got[4];memcpy(got,vu,sizeof(got));
        assert(got[0]==0x12345678u && got[1]==0x12345678u &&
               got[2]==0x12345678u && got[3]==0x12345678u);
        assert(ms.qwords_written==1u);
    }
    {
        /* Sequential UNPACK writes wrap after the final VU1 quadword. */
        uint8_t m[32]={0};uint8_t large_vu[16384]={0};
        uint32_t head[]={v(0x60,2,0x03ff),0x11111111u,0x22222222u};
        memcpy(m,head,sizeof(head));memset(&ms,0,sizeof(ms));
        assert(tsfp_vif_unpack_memory(m,sizeof(head),large_vu,sizeof(large_vu),&ms)==0);
        uint32_t last[4],first[4];
        memcpy(last,large_vu+0x3ff0,sizeof(last));memcpy(first,large_vu,sizeof(first));
        assert(last[0]==0x11111111u && last[1]==0x11111111u &&
               last[2]==0x11111111u && last[3]==0x11111111u);
        assert(first[0]==0x22222222u && first[1]==0x22222222u &&
               first[2]==0x22222222u && first[3]==0x22222222u);
        assert(ms.qwords_written==2u);
    }
    return 0;
}
