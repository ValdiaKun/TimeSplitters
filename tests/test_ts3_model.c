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
    return 0;
}
