#ifndef FP_GS_VIEW_H
#define FP_GS_VIEW_H

#include <stddef.h>
#include "fp_gif.h"

typedef struct {
    float center_x;
    float center_y;
    float scale;
    float screen_width;
    float screen_height;
} TsFpGsView;

/* Fit already-transformed GS-space XYZ vertices to a Vita viewport. */
int tsfp_gs_view_fit(TsFpGsView *view,const TsFpGifVertex *vertices,size_t count,
                     float screen_width,float screen_height);
int tsfp_gs_view_project(const TsFpGsView *view,const TsFpGifVertex *vertex,
                         float zoom,float pan_x,float pan_y,float *x,float *y);
/* Return the mean depth for a finite triangle without overflowing float math. */
int tsfp_gs_triangle_depth(const TsFpGifVertex *a,const TsFpGifVertex *b,
                           const TsFpGifVertex *c,double *depth);

#endif
