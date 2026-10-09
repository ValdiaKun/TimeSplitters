#include "fp_scene.h"
#include <math.h>
#include <float.h>
#include <string.h>

void tsfp_scene_bounds_reset(TsFpSceneBounds *b){
    if(!b)return;
    b->min_x=b->min_y=b->min_z=FLT_MAX;
    b->max_x=b->max_y=b->max_z=-FLT_MAX;
    b->center_x=b->center_y=b->center_z=0.0f;
    b->radius=0.0f;
}

void tsfp_scene_bounds_add(TsFpSceneBounds *b,float x,float y,float z){
    if(!b||!isfinite(x)||!isfinite(y)||!isfinite(z))return;
    if(x<b->min_x)b->min_x=x;
    if(x>b->max_x)b->max_x=x;
    if(y<b->min_y)b->min_y=y;
    if(y>b->max_y)b->max_y=y;
    if(z<b->min_z)b->min_z=z;
    if(z>b->max_z)b->max_z=z;
    /* Double intermediates prevent overflow when finite float bounds are added. */
    b->center_x=(float)(((double)b->min_x+(double)b->max_x)*0.5);
    b->center_y=(float)(((double)b->min_y+(double)b->max_y)*0.5);
    b->center_z=(float)(((double)b->min_z+(double)b->max_z)*0.5);
    double dx=(double)b->max_x-(double)b->min_x;
    double dy=(double)b->max_y-(double)b->min_y;
    double dz=(double)b->max_z-(double)b->min_z;
    double r=0.5*sqrt(dx*dx+dy*dy+dz*dz);
    b->radius=r>(double)FLT_MAX?FLT_MAX:(float)r;
}

void tsfp_scene_camera_fit(TsFpSceneCamera *c,const TsFpSceneBounds *b,float width,float height){
    if(!c||!b)return;
    memset(c,0,sizeof(*c));
    if(!isfinite(width)||!isfinite(height)||width<=0.0f||height<=0.0f){
        c->distance=1.0f;
        return;
    }
    c->screen_width=width;c->screen_height=height;
    c->focal=0.5f*(width<height?width:height);
    double distance=isfinite(b->radius)&&b->radius>0.001f?(double)b->radius*2.5:1.0;
    c->distance=distance>(double)FLT_MAX?FLT_MAX:(float)distance;
}

TsFpScenePoint tsfp_scene_project(const TsFpSceneCamera *c,const TsFpSceneBounds *b,float x,float y,float z){
    TsFpScenePoint p={0,0,0,0};
    if(!c||!b||!isfinite(x)||!isfinite(y)||!isfinite(z)||
       !isfinite(b->center_x)||!isfinite(b->center_y)||!isfinite(b->center_z)||
       !isfinite(c->screen_width)||!isfinite(c->screen_height)||!isfinite(c->focal)||
       c->screen_width<=0.0f||c->screen_height<=0.0f||c->focal<=0.0f||
       !isfinite(c->yaw)||!isfinite(c->pitch)||!isfinite(c->distance)||c->distance<=0.001f)return p;
    double px=(double)x-(double)b->center_x;
    double py=(double)y-(double)b->center_y;
    double pz=(double)z-(double)b->center_z;
    double sy=sin((double)c->yaw),cy=cos((double)c->yaw);
    double x1=px*cy-pz*sy;
    double z1=px*sy+pz*cy;
    double sp=sin((double)c->pitch),cp=cos((double)c->pitch);
    double y1=py*cp-z1*sp;
    double z2=py*sp+z1*cp+(double)c->distance;
    if(!isfinite(z2)||z2<=0.001||z2>(double)FLT_MAX)return p;
    p.depth=(float)z2;
    double scale=(double)c->focal/z2;
    double screen_x=(double)c->screen_width*0.5+x1*scale;
    double screen_y=(double)c->screen_height*0.5-y1*scale;
    if(!isfinite(screen_x)||!isfinite(screen_y)||
       screen_x<-(double)FLT_MAX||screen_x>(double)FLT_MAX||
       screen_y<-(double)FLT_MAX||screen_y>(double)FLT_MAX)return p;
    p.x=(float)screen_x;
    p.y=(float)screen_y;
    p.visible=1;
    return p;
}
