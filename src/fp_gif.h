#ifndef FP_GIF_H
#define FP_GIF_H
#include <stddef.h>
#include <stdint.h>
/* XYZ fields are GS-space screen coordinates/depth emitted by VU1, not object-space floats. */
typedef struct { float x,y,z,s,t; uint8_t r,g,b,a,skip,primitive; } TsFpGifVertex;
typedef struct { uint32_t tags,loops,vertices; uint32_t primitive; uint8_t format,registers; size_t bytes_consumed; } TsFpGifSummary;
typedef struct {
    float s,t;
    uint8_t r,g,b,a;
    uint32_t primitive;
    uint8_t primitive_valid;
} TsFpGifState;
int tsfp_gif_parse_state(const uint8_t *data,size_t size,TsFpGifSummary *out,
                         TsFpGifVertex *vertices,size_t vertex_capacity,TsFpGifState *state);
int tsfp_gif_parse(const uint8_t *data,size_t size,TsFpGifSummary *out,TsFpGifVertex *vertices,size_t vertex_capacity);
size_t tsfp_gif_triangulate(TsFpGifVertex *dst,size_t capacity,const TsFpGifVertex *src,size_t count,uint32_t primitive);
#endif
