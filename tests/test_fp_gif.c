#include "fp_gif.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
static void w64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(i*8));}
int main(void){uint8_t d[80]={0};uint64_t tag=1ull|(1ull<<15)|(0ull<<58)|(3ull<<60);uint64_t regs=0x521ull;w64(d,tag);w64(d+8,regs);w64(d+16,0x0000000000000000ull);w64(d+24,0x3f8000003f000000ull);w64(d+32,0x00000000000000ffull);w64(d+40,0x0000000000100000ull);w64(d+48,0x0000000002000100ull);w64(d+56,0x0000000000000080ull);TsFpGifSummary s;TsFpGifVertex v;assert(tsfp_gif_parse(d,sizeof(d),&s,&v,1)==0);assert(s.tags==1&&s.vertices==1&&s.loops==1);assert(v.x==16.0f && v.y==32.0f && v.z==128.0f);
    {
        uint8_t r[32]={0};
        uint64_t rtag=1ull|(1ull<<15)|(1ull<<58)|(1ull<<60);
        w64(r,rtag); w64(r+8,5ull);
        w64(r+16,((uint64_t)128u<<32)|((uint64_t)32u<<16)|16u);
        w64(r+24,0);
        TsFpGifSummary rs; TsFpGifVertex rv;
        assert(tsfp_gif_parse(r,sizeof(r),&rs,&rv,1)==0);
        assert(rs.tags==1&&rs.vertices==1&&rs.loops==1);
        assert(rv.x==1.0f&&rv.y==2.0f&&rv.z==128.0f);
    }
    {
        /* XYZF2 (GIF register 0x04) is a vertex-kick register like XYZ2,
           with a 24-bit Z and an 8-bit fog field. */
        uint8_t f[32]={0};
        uint64_t ftag=1ull|(1ull<<15)|(0ull<<58)|(1ull<<60);
        w64(f,ftag); w64(f+8,4ull);
        w64(f+16,((uint64_t)0x0020u<<16)|0x0010u);
        w64(f+24,0x12abcdefu);
        TsFpGifSummary fs; TsFpGifVertex fv;
        assert(tsfp_gif_parse(f,sizeof(f),&fs,&fv,1)==0);
        assert(fs.vertices==1 && fv.x==1.0f && fv.y==2.0f && (uint32_t)fv.z==0xabcdefu);
    }
    return 0;}
