#ifndef FP_GEOMETRY_H
#define FP_GEOMETRY_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t data_offset;
    uint16_t vertex_count;
    uint16_t flags;
} TsFpSubmesh;

typedef struct {
    uint32_t table_offset;
    uint32_t mesh_count;
    uint32_t submesh_count;
    uint32_t invalid_count;
} TsFpGeometrySummary;

int tsfp_geometry_probe(const uint8_t *data, size_t size,
                        uint32_t table_offset, uint32_t mesh_count,
                        TsFpGeometrySummary *out);

/* Collect structurally valid submesh records in mesh-table order. */
size_t tsfp_geometry_collect(const uint8_t *data, size_t size,
                             uint32_t table_offset, uint32_t mesh_count,
                             TsFpSubmesh *out, size_t capacity);

#endif
