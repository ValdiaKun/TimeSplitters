#include "fp_vif.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static TsFpVifCommand decode(uint32_t v) {
    TsFpVifCommand c;
    c.command = (uint8_t)(v >> 24);
    c.num = (uint8_t)(v >> 16);
    c.immediate = (uint16_t)v;
    return c;
}

int tsfp_vif_probe(const uint8_t *data, size_t size, TsFpVifSummary *out) {
    if (!data || !out || size < 36) return -1;

    memset(out, 0, sizeof(*out));
    out->stcycl = decode(rd32(data + 0));
    out->unpack = decode(rd32(data + 4));
    out->strow = decode(rd32(data + 12));

    if (out->stcycl.command != 0x01) return -2;
    if (out->unpack.command != 0x6c) return -3; /* V4-32 */
    if (out->unpack.num == 0) return -4;
    if (out->strow.command != 0x30) return -5;

    for (uint32_t i = 0; i < 4; ++i)
        out->row[i] = rd32(data + 16 + i * 4);

    out->payload_offset = 32;
    out->unpack_vectors = out->unpack.num;
    out->payload_bytes = out->unpack_vectors * 16u;
    if (out->payload_bytes > size - out->payload_offset) return -6;
    return 0;
}
