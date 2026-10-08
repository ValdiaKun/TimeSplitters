#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ts3_model.h"

int main(void) {
    uint8_t d[160]; memset(d,0,sizeof(d));
    uint32_t mo=48, io=96;
    memcpy(d,&mo,4); memcpy(d+4,&io,4);
    int32_t meshes=3; memcpy(d+io,&meshes,4);
    float scale=1.25f; memcpy(d+io+12,&scale,4);
    uint32_t sentinel=0xFFFFFFFFu; memcpy(d+mo+16,&sentinel,4);
    Ts3ModelSummary s;
    assert(ts3_model_probe(d,sizeof(d),&s)==0);
    assert(s.info.num_submeshes==3);
    assert(s.material_count==1);

    /* Truncated info and unterminated material tables must fail cleanly. */
    uint8_t malformed[160]; memset(malformed,0,sizeof(malformed));
    uint32_t bad_mo=48,bad_io=128;
    memcpy(malformed,&bad_mo,4);memcpy(malformed+4,&bad_io,4);
    assert(ts3_model_probe(malformed,sizeof(malformed),&s)==-3);

    memset(malformed,0,sizeof(malformed));
    bad_mo=144;bad_io=64;
    memcpy(malformed,&bad_mo,4);memcpy(malformed+4,&bad_io,4);
    memcpy(malformed+bad_io,&meshes,4);
    memcpy(malformed+bad_io+12,&scale,4);
    assert(ts3_model_probe(malformed,sizeof(malformed),&s)==-6);
    return 0;
}
