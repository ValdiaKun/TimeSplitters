#ifndef FP_RESOURCE_H
#define FP_RESOURCE_H

#include <stdint.h>
#include <stddef.h>

#define TSFP_RESOURCE_MAGIC 0x408e6ec4u

typedef struct {
    uint32_t total_size;
    uint32_t string_bytes;
    uint32_t string_count;
    uint32_t metadata_offset;
} TsFpResourceSummary;

int tsfp_resource_probe(const uint8_t *data, size_t size, TsFpResourceSummary *out);

#endif
