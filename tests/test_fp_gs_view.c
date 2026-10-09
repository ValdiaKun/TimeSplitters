#include "fp_gs_view.h"
#include <assert.h>
#include <float.h>
#include <math.h>

int main(void){
    TsFpGifVertex vertices[3]={
        {.x=0.0f,.y=0.0f,.z=10.0f},
        {.x=100.0f,.y=50.0f,.z=20.0f},
        {.x=50.0f,.y=25.0f,.z=15.0f}
    };
    TsFpGsView view;
    assert(tsfp_gs_view_fit(&view,vertices,3,960.0f,544.0f)==0);
    assert(fabsf(view.center_x-50.0f)<0.001f);
    assert(fabsf(view.center_y-25.0f)<0.001f);
    assert(fabsf(view.scale-7.68f)<0.001f);

    float x=0.0f,y=0.0f;
    assert(tsfp_gs_view_project(&view,&vertices[0],1.0f,0.0f,0.0f,&x,&y));
    assert(fabsf(x-96.0f)<0.01f && fabsf(y-80.0f)<0.01f);
    assert(tsfp_gs_view_project(&view,&vertices[1],1.0f,0.0f,0.0f,&x,&y));
    assert(fabsf(x-864.0f)<0.01f && fabsf(y-464.0f)<0.01f);
    assert(tsfp_gs_view_project(&view,&vertices[2],2.0f,10.0f,-5.0f,&x,&y));
    assert(fabsf(x-490.0f)<0.02f && fabsf(y-267.0f)<0.02f);

    TsFpGifVertex invalid=vertices[0];
    invalid.z=NAN;
    assert(!tsfp_gs_view_project(&view,&invalid,1.0f,0.0f,0.0f,&x,&y));
    assert(!tsfp_gs_view_project(&view,&vertices[0],0.0f,0.0f,0.0f,&x,&y));
    assert(!tsfp_gs_view_project(&view,&vertices[0],1.0f,NAN,0.0f,&x,&y));
    assert(!tsfp_gs_view_project(NULL,&vertices[0],1.0f,0.0f,0.0f,&x,&y));
    assert(tsfp_gs_view_fit(&view,vertices,3,0.0f,544.0f)==-1);

    /* Non-finite vertices are ignored for fitting; all-invalid input fails. */
    TsFpGifVertex mixed[4]={vertices[0],vertices[1],vertices[2],{.x=NAN,.y=0.0f,.z=1.0f}};
    assert(tsfp_gs_view_fit(&view,mixed,4,960.0f,544.0f)==0);
    assert(fabsf(view.center_x-50.0f)<0.001f && fabsf(view.scale-7.68f)<0.001f);
    TsFpGifVertex invalid_set={.x=NAN,.y=NAN,.z=NAN};
    assert(tsfp_gs_view_fit(&view,&invalid_set,1,960.0f,544.0f)==-2);

    /* A valid double scale can underflow to zero when converted to float. */
    TsFpGifVertex extreme_range[2]={
        {.x=0.0f,.y=-FLT_MAX*0.5f,.z=0.0f},
        {.x=1.0f,.y=FLT_MAX*0.5f,.z=0.0f}
    };
    TsFpGsView unchanged={1.0f,2.0f,3.0f,4.0f,5.0f};
    assert(tsfp_gs_view_fit(&unchanged,extreme_range,2,100.0f,FLT_MIN)==-3);
    assert(unchanged.center_x==1.0f && unchanged.center_y==2.0f && unchanged.scale==3.0f &&
           unchanged.screen_width==4.0f && unchanged.screen_height==5.0f);

    /* A degenerate point cloud remains centered and projectable. */
    TsFpGifVertex point={.x=4.0f,.y=5.0f,.z=6.0f};
    assert(tsfp_gs_view_fit(&view,&point,1,320.0f,240.0f)==0);
    assert(view.scale==1.0f);
    assert(tsfp_gs_view_project(&view,&point,1.0f,0.0f,0.0f,&x,&y));
    assert(fabsf(x-160.0f)<0.001f && fabsf(y-120.0f)<0.001f);

    /* Averaging finite triangle depths in float can overflow before the
       positive and negative values cancel. */
    TsFpGifVertex depth_vertices[3]={
        {.z=FLT_MAX}, {.z=FLT_MAX}, {.z=-FLT_MAX}
    };
    float old_depth=(depth_vertices[0].z+depth_vertices[1].z+depth_vertices[2].z)*(1.0f/3.0f);
    assert(!isfinite(old_depth));
    double depth=0.0;
    assert(tsfp_gs_triangle_depth(&depth_vertices[0],&depth_vertices[1],
                                  &depth_vertices[2],&depth));
    assert(isfinite(depth) && depth>=(double)FLT_MAX*0.3 && depth<=(double)FLT_MAX*0.4);
    depth_vertices[1].z=NAN;
    double previous_depth=depth;
    assert(!tsfp_gs_triangle_depth(&depth_vertices[0],&depth_vertices[1],
                                   &depth_vertices[2],&depth));
    assert(depth==previous_depth);
    return 0;
}
