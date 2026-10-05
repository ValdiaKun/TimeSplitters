#ifndef FP_GIF_H
#define FP_GIF_H
#include <stddef.h>
#include <stdint.h>
typedef struct { float x,y,z,s,t; uint8_t r,g,b,a; } TsFpGifVertex;
typedef struct { uint32_t tags,loops,vertices; uint32_t primitive; uint8_t format,registers; size_t bytes_consumed; } TsFpGifSummary;
int tsfp_gif_parse(const uint8_t *data,size_t size,TsFpGifSummary *out,TsFpGifVertex *vertices,size_t vertex_capacity);
#endif
