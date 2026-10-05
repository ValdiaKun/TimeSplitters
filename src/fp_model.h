#ifndef FP_MODEL_H
#define FP_MODEL_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t material_offset;
    uint32_t info_offset;
    uint32_t auxiliary_offset;
    uint32_t mesh_count;
    uint32_t material_count;
    uint32_t lod_count;
    float scale;
} TsFpModelHeader;
int tsfp_model_probe(const uint8_t *data, size_t size, TsFpModelHeader *out);
#endif
