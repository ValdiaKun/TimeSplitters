#include "ts3_model.h"
#include <string.h>
#include <math.h>

static uint32_t u32(const uint8_t *p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t i32(const uint8_t *p) { return (int32_t)u32(p); }
static float f32(const uint8_t *p) {
    uint32_t v=u32(p); float x; memcpy(&x,&v,4); return x;
}
int ts3_model_probe(const uint8_t *d, size_t n, Ts3ModelSummary *o) {
    if (!d || !o || n < 48) return -1;
    uint32_t mo=u32(d), io=u32(d+4), uo=u32(d+8);
    if (mo >= n || io >= n || (uo && uo >= n) || (mo & 3u) || (io & 3u)) return -2;
    if (io + 36 > n) return -3;
    Ts3ModelInfo info;
    memset(&info,0,sizeof(info));
    info.num_submeshes=i32(d+io);
    memcpy(info.unk,d+io+4,8);
    info.scale=f32(d+io+12);
    info.unk2=i32(d+io+16);
    for(int i=0;i<3;i++) info.unk_floats[i]=f32(d+io+20+i*4);
    for(int i=0;i<2;i++) info.unk3[i]=i32(d+io+32+i*4);
    if (info.num_submeshes < 0 || info.num_submeshes > 128) return -4;
    if (!isfinite(info.scale) || fabsf(info.scale) < 0.000001f || fabsf(info.scale) > 100000.0f) return -5;
    uint32_t mats=0;
    for (size_t p=mo; p+16<=n && mats<4096; p+=16) {
        if (u32(d+p)==0xFFFFFFFFu) break;
        mats++;
    }
    if (mo + (mats+1)*16 > n) return -6;
    o->mat_info_offset=mo; o->info_offset=io; o->unk_offset=uo; o->info=info; o->material_count=mats;
    return 0;
}
