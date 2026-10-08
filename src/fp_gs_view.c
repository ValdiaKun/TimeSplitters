#include "fp_gs_view.h"
#include <float.h>
#include <math.h>

int tsfp_gs_view_fit(TsFpGsView *view,const TsFpGifVertex *vertices,size_t count,
                     float screen_width,float screen_height){
    if(!view||!vertices||!count||!isfinite(screen_width)||!isfinite(screen_height)||
       screen_width<=0.0f||screen_height<=0.0f)return -1;
    double min_x=DBL_MAX,min_y=DBL_MAX,max_x=-DBL_MAX,max_y=-DBL_MAX;
    for(size_t i=0;i<count;i++){
        double x=vertices[i].x,y=vertices[i].y;
        if(!isfinite(x)||!isfinite(y)||!isfinite(vertices[i].z))continue;
        if(x<min_x)min_x=x;
        if(x>max_x)max_x=x;
        if(y<min_y)min_y=y;
        if(y>max_y)max_y=y;
    }
    if(min_x==DBL_MAX||min_y==DBL_MAX)return -2;
    double range_x=max_x-min_x,range_y=max_y-min_y;
    double scale_x=range_x>0.0?(double)screen_width*0.8/range_x:DBL_MAX;
    double scale_y=range_y>0.0?(double)screen_height*0.8/range_y:DBL_MAX;
    double scale;
    if(range_x<=0.0&&range_y<=0.0)scale=1.0;
    else if(range_x<=0.0)scale=scale_y;
    else if(range_y<=0.0)scale=scale_x;
    else scale=scale_x<scale_y?scale_x:scale_y;
    if(!isfinite(scale)||scale<=0.0)return -3;
    view->center_x=(float)(min_x+range_x*0.5);
    view->center_y=(float)(min_y+range_y*0.5);
    view->scale=scale>(double)FLT_MAX?FLT_MAX:(float)scale;
    view->screen_width=screen_width;
    view->screen_height=screen_height;
    return 0;
}

int tsfp_gs_view_project(const TsFpGsView *view,const TsFpGifVertex *vertex,
                         float zoom,float pan_x,float pan_y,float *x,float *y){
    if(!view||!vertex||!x||!y||!isfinite(vertex->x)||!isfinite(vertex->y)||!isfinite(vertex->z)||
       !isfinite(view->center_x)||!isfinite(view->center_y)||!isfinite(view->scale)||
       !isfinite(view->screen_width)||!isfinite(view->screen_height)||
       !isfinite(zoom)||!isfinite(pan_x)||!isfinite(pan_y)||
       view->scale<=0.0f||view->screen_width<=0.0f||view->screen_height<=0.0f||zoom<=0.0f)return 0;
    double sx=(double)view->screen_width*0.5+
              ((double)vertex->x-view->center_x)*(double)view->scale*zoom+pan_x;
    double sy=(double)view->screen_height*0.5+
              ((double)vertex->y-view->center_y)*(double)view->scale*zoom+pan_y;
    if(!isfinite(sx)||!isfinite(sy)||sx>FLT_MAX||sx< -FLT_MAX||sy>FLT_MAX||sy< -FLT_MAX)return 0;
    *x=(float)sx;*y=(float)sy;
    return isfinite(*x)&&isfinite(*y);
}
