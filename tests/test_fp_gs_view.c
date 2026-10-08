#include "fp_gs_view.h"
#include <assert.h>
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
    assert(tsfp_gs_view_project(&view,&vertices[0],2.0f,10.0f,-5.0f,&x,&y));
    assert(fabsf(x-202.0f)<0.02f && fabsf(y-83.0f)<0.02f);

    TsFpGifVertex invalid=vertices[0];
    invalid.z=NAN;
    assert(!tsfp_gs_view_project(&view,&invalid,1.0f,0.0f,0.0f,&x,&y));
    assert(!tsfp_gs_view_project(&view,&vertices[0],0.0f,0.0f,0.0f,&x,&y));
    assert(!tsfp_gs_view_project(NULL,&vertices[0],1.0f,0.0f,0.0f,&x,&y));
    assert(tsfp_gs_view_fit(&view,vertices,3,0.0f,544.0f)==-1);

    /* A degenerate point cloud remains centered and projectable. */
    TsFpGifVertex point={.x=4.0f,.y=5.0f,.z=6.0f};
    assert(tsfp_gs_view_fit(&view,&point,1,320.0f,240.0f)==0);
    assert(view.scale==1.0f);
    assert(tsfp_gs_view_project(&view,&point,1.0f,0.0f,0.0f,&x,&y));
    assert(fabsf(x-160.0f)<0.001f && fabsf(y-120.0f)<0.001f);
    return 0;
}
