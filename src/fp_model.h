#ifndef FP_MODEL_H
#define FP_MODEL_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t chunk_a, chunk_b, chunk_c, flags; } TsFpModelHeader;
int tsfp_model_probe(const uint8_t *data, size_t size, TsFpModelHeader *out);
#endif
