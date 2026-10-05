#include "fp_vif.h"
#include <string.h>

static uint32_t unpack_word_bytes(uint8_t format) {
    switch (format & 0x0f) {
        case 0x0c: return 16; /* V4-32 */
        case 0x08: return 16; /* V3-32, padded to a qword */
        case 0x04: return 16; /* V2-32, padded to a qword */
        case 0x00: return 4;  /* S-32 */
        case 0x0d: return 8;  /* V4-16 */
        case 0x09: return 8;  /* V3-16, padded */
        case 0x05: return 8;  /* V2-16, padded */
        case 0x01: return 2;  /* S-16 */
        case 0x0e: return 4;  /* V4-8 */
        case 0x0a: return 4;  /* V3-8, padded */
        case 0x06: return 4;  /* V2-8, padded */
        case 0x02: return 1;  /* S-8 */
        case 0x0f: return 4;  /* V4-5, packed into words */
        default: return 0;
    }
}

static uint32_t unpack_words_per_vector(uint8_t format) {
    switch (format & 0x0f) {
        case 0x0c: return 4; case 0x08: return 4; case 0x04: return 4;
        case 0x00: return 1; case 0x0d: return 2; case 0x09: return 2;
        case 0x05: return 2; case 0x01: return 1; case 0x0e: return 1;
        case 0x0a: return 1; case 0x06: return 1; case 0x02: return 1;
        case 0x0f: return 1; default: return 0;
    }
}

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

    out->cycle_length = (uint8_t)(out->stcycl.immediate & 0xffu);
    out->write_length = (uint8_t)(out->stcycl.immediate >> 8);
    out->unpack_address = (uint32_t)(out->unpack.immediate & 0x3ffu);
    out->unpack_unsigned = (uint8_t)((out->unpack.immediate >> 14) & 1u);
    out->unpack_top_relative = (uint8_t)((out->unpack.immediate >> 15) & 1u);
    out->payload_offset = 32;
    out->unpack_vectors = out->unpack.num;
    out->payload_bytes = out->unpack_vectors * 16u;
    if (out->payload_bytes > size - out->payload_offset) return -6;
    for (uint32_t i = 0; i < 4; ++i)
        out->vector[i] = rd32(data + out->payload_offset + i * 4u);
    return 0;
}

int tsfp_vif_scan(const uint8_t *data, size_t size, TsFpVifSummary *out) {
    size_t pos = 0;
    if (!data || !out || size < 4) return -1;
    if (tsfp_vif_probe(data, size, out) != 0) return -2;

    while (pos + 4 <= size) {
        uint32_t w = rd32(data + pos);
        uint8_t cmd = (uint8_t)(w >> 24);
        uint8_t num = (uint8_t)(w >> 16);
        uint16_t imm = (uint16_t)w;
        out->command_count++;
        pos += 4;

        if (cmd == 0x00) continue;
        if (cmd == 0x01 || cmd == 0x02 || cmd == 0x03 || cmd == 0x04 ||
            cmd == 0x05 || cmd == 0x06 || cmd == 0x07 ||
            cmd == 0x10 || cmd == 0x11 || cmd == 0x13 ||
            cmd == 0x14 || cmd == 0x15 || cmd == 0x17) {
            if (cmd == 0x14 || cmd == 0x15) out->mscal_count++;
            continue;
        }
        if (cmd == 0x20) { if (pos + 4 > size) return -3; pos += 4; continue; }
        if (cmd == 0x30 || cmd == 0x31) {
            if (pos + 16 > size) return -4;
            pos += 16;
            continue;
        }
        if ((cmd & 0xE0u) == 0x60u) {
            uint32_t bytes = unpack_word_bytes(cmd);
            uint32_t words = unpack_words_per_vector(cmd);
            if (!bytes || !words) return -5;
            uint32_t vectors = num ? num : 256;
            uint32_t consumed = ((vectors * words * 4u) + 15u) & ~15u;
            if (consumed > size - pos) return -6;
            out->unpack_count++;
            out->unpack_qwords += (consumed / 16u);
            out->unpack_data_bytes += vectors * bytes;
            pos += consumed;
            (void)imm;
            continue;
        }
        if (cmd == 0x4a) {
            uint32_t bytes = (num ? num : 256u) * 8u;
            if (bytes > size - pos) return -7;
            pos += bytes;
            continue;
        }
        if (cmd == 0x50 || cmd == 0x51) {
            uint32_t bytes = (uint32_t)imm * 16u;
            if (bytes > size - pos) return -8;
            pos += bytes;
            continue;
        }
        return -9;
    }
    return pos == size ? 0 : -10;
}
