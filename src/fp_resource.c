#include "fp_resource.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int tsfp_resource_probe(const uint8_t *data, size_t size, TsFpResourceSummary *out) {
    size_t p, strings_end, count = 0;
    uint32_t total;

    if (!data || !out || size < 8) return -1;
    if (rd32(data) != TSFP_RESOURCE_MAGIC) return -2;

    total = rd32(data + 4);
    if (total < 8 || total > size) return -3;

    p = 8;
    strings_end = p;
    while (p < total) {
        size_t start = p;
        while (p < total && data[p] != 0) p++;
        if (p == total) break;
        if (p > start) count++;
        p++;
        strings_end = p;
        if (p + 4 <= total && data[p] == 0xff && data[p+1] == 0xff &&
            data[p+2] == 0xff && data[p+3] == 0xff) break;
    }

    memset(out, 0, sizeof(*out));
    out->total_size = total;
    out->string_bytes = (uint32_t)(strings_end - 8);
    out->string_count = (uint32_t)count;
    out->metadata_offset = (uint32_t)strings_end;
    return 0;
}
