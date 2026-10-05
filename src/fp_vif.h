#ifndef FP_VIF_H
#define FP_VIF_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t command;
    uint8_t num;
    uint16_t immediate;
} TsFpVifCommand;

typedef struct {
    TsFpVifCommand stcycl;
    TsFpVifCommand unpack;
    TsFpVifCommand strow;
    uint32_t row[4];
    uint32_t payload_offset;
    uint32_t payload_bytes;
    uint32_t unpack_vectors;
} TsFpVifSummary;

int tsfp_vif_probe(const uint8_t *data, size_t size, TsFpVifSummary *out);
#endif
