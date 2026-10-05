#include "fp_resource.h"
#include <assert.h>
#include <stdint.h>

int main(void) {
    uint8_t d[32] = {
        0xc4,0x6e,0x8e,0x40, 0x20,0,0,0,
        'R','A','I','L','B','O','T',0,
        'r','f','a','c','t','o','r','y',0,
        0xff,0xff,0xff,0xff
    };
    TsFpResourceSummary s;
    assert(tsfp_resource_probe(d,sizeof(d),&s)==0);
    assert(s.total_size==32);
    assert(s.string_count>=2);
    assert(s.metadata_offset>8);
    return 0;
}
