#include "fp_gif.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
static void w64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(i*8));}
int main(void){uint8_t d[80]={0};uint64_t tag=1ull|(1ull<<15)|(0ull<<58)|(3ull<<60);uint64_t regs=0x521ull;w64(d,tag);w64(d+8,regs);w64(d+16,0x0000000000000000ull);w64(d+24,0x3f8000003f000000ull);w64(d+32,0x00000000000000ffull);w64(d+40,0x0000000000100000ull);w64(d+48,0x0000000000000000ull);w64(d+56,0x000000003f800000ull);TsFpGifSummary s;TsFpGifVertex v;assert(tsfp_gif_parse(d,sizeof(d),&s,&v,1)==0);assert(s.tags==1&&s.vertices==1&&s.loops==1);assert(v.z==1.0f);return 0;}
