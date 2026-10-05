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
    uint32_t unpack_address;
    uint8_t unpack_unsigned;
    uint8_t unpack_top_relative;
    uint16_t cycle_length;
    uint16_t write_length;
    uint32_t vector[4];
    uint32_t command_count;
    uint32_t unpack_count;
    uint32_t unpack_qwords;
    uint32_t unpack_data_bytes;
    uint32_t mscal_count;
} TsFpVifSummary;

int tsfp_vif_probe(const uint8_t *data, size_t size, TsFpVifSummary *out);
int tsfp_vif_scan(const uint8_t *data, size_t size, TsFpVifSummary *out);
#endif


typedef struct {
    uint32_t bytes_written;
    uint32_t qwords_written;
    uint32_t unpack_commands;
    uint32_t mscal_address;
} TsFpVifMemorySummary;

int tsfp_vif_unpack_memory(const uint8_t *data, size_t size,
                           uint8_t *vu_memory, size_t vu_size,
                           TsFpVifMemorySummary *out);
