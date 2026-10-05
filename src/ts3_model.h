#ifndef TS3_MODEL_H
#define TS3_MODEL_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int32_t tex_id;
    int32_t tex_id2;
    int32_t unk;
    int32_t unk2;
} Ts3MatInfo;

typedef struct {
    int32_t num_submeshes;
    uint8_t unk[8];
    float scale;
    int32_t unk2;
    float unk_floats[3];
    int32_t unk3[2];
} Ts3ModelInfo;

typedef struct {
    uint32_t mat_info_offset;
    uint32_t info_offset;
    uint32_t unk_offset;
    Ts3ModelInfo info;
    uint32_t material_count;
} Ts3ModelSummary;

int ts3_model_probe(const uint8_t *data, size_t size, Ts3ModelSummary *out);

#endif
