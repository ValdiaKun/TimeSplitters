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

    out->cycle_length = (uint16_t)(out->stcycl.immediate & 0xffu);
    out->write_length = (uint16_t)(out->stcycl.immediate >> 8);
    if (out->cycle_length == 0) out->cycle_length = 256;
    if (out->write_length == 0) out->write_length = 256;
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
    memset(out, 0, sizeof(*out));

    uint32_t scan_cl = 256u;
    uint32_t scan_wl = 256u;
    uint32_t scan_cycle_pos = 0;

    while (pos + 4 <= size) {
        uint32_t w = rd32(data + pos);
        uint8_t cmd = (uint8_t)(w >> 24);
        uint8_t num = (uint8_t)(w >> 16);
        uint16_t imm = (uint16_t)w;
        out->command_count++;
        pos += 4;

        if (cmd == 0x00) continue;
        if (cmd == 0x01) {
            if (out->cycle_length == 0 && out->write_length == 0)
                out->stcycl = decode(w);
            scan_cl = (uint32_t)(imm & 0xffu);
            scan_wl = (uint32_t)(imm >> 8);
            if (!scan_cl) scan_cl = 256u;
            if (!scan_wl) scan_wl = 256u;
            out->cycle_length = (uint16_t)scan_cl;
            out->write_length = (uint16_t)scan_wl;
            scan_cycle_pos = 0;
            continue;
        }
        if (cmd == 0x02 || cmd == 0x03 || cmd == 0x04 ||
            cmd == 0x05 || cmd == 0x06 || cmd == 0x07 ||
            cmd == 0x10 || cmd == 0x11 || cmd == 0x13 ||
            cmd == 0x14 || cmd == 0x15 || cmd == 0x17) {
            if (cmd == 0x14 || cmd == 0x15) {
                out->mscal_count++;
                out->mscal_address = imm;
            }
            continue;
        }
        if (cmd == 0x20) { if (pos + 4 > size) return -3; pos += 4; continue; }
        if (cmd == 0x30 || cmd == 0x31) {
            if (pos + 16 > size) return -4;
            if (out->strow.command == 0 && cmd == 0x30) {
                out->strow = decode(w);
                for (uint32_t i = 0; i < 4; ++i)
                    out->row[i] = rd32(data + pos + i * 4u);
            }
            pos += 16;
            continue;
        }
        if ((cmd & 0xE0u) == 0x60u) {
            uint32_t format = cmd & 0x0fu;
            if (out->unpack.command == 0) {
                out->unpack = decode(w);
                out->payload_offset = (uint32_t)pos;
                out->unpack_vectors = num ? num : 256u;
                out->unpack_unsigned = (uint8_t)((imm >> 14) & 1u);
                out->unpack_top_relative = (uint8_t)((imm >> 15) & 1u);
                out->unpack_address = (uint32_t)(imm & 0x3ffu);
            }
            uint32_t bits = (format == 0x0fu) ? 20u :
                (32u >> (format & 3u)) * (((format >> 2) & 3u) + 1u);
            uint32_t words = (bits + 31u) / 32u;
            if (!bits || !words) return -5;
            uint32_t vectors = num ? num : 256;
            /*
             * NUM counts output vectors, but when WL > CL the VIF fills the
             * remaining slots in each cycle from the last consumed vector.
             * Only CL vectors per cycle therefore consume source payload.
             */
            uint32_t cl = scan_cl;
            uint32_t wl = scan_wl;
            uint32_t cycle_len = wl > cl ? wl : cl;
            uint32_t source_vectors = 0;
            uint32_t destination_qwords = 0;
            uint32_t consumed = 0;
            /*
             * STCYCL state spans UNPACK commands.  Advance the cycle position
             * for every output vector, consuming source data only in the
             * write-length portion of each cycle.  This mirrors the executor
             * and correctly handles an UNPACK that begins mid-cycle.
             */
            for (uint32_t v = 0; v < vectors; ++v) {
                if (scan_cycle_pos < wl) destination_qwords++;
                if ((wl > cl && scan_cycle_pos < cl) ||
                    (wl <= cl && scan_cycle_pos < wl)) {
                    source_vectors++;
                    consumed += words * 4u;
                }
                scan_cycle_pos++;
                if (scan_cycle_pos >= cycle_len)
                    scan_cycle_pos = 0;
            }
            if (consumed > size - pos) return -6;
            out->unpack_count++;
            out->unpack_qwords += destination_qwords;
            out->unpack_data_bytes += source_vectors * ((bits + 7u) / 8u);
            if (out->unpack.command == cmd && out->payload_bytes == 0) {
                out->payload_bytes = source_vectors * words * 4u;
                if (out->unpack.command == 0x6cu && source_vectors > 0 && pos + 16u <= size)
                    for (unsigned i = 0; i < 4; ++i)
                        out->vector[i] = rd32(data + pos + i * 4u);
            }
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
