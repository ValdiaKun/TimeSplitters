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
       !isfinite(c->screen_width)||!isfinite(c->screen_height)||!isfinite(c->focal)||
       c->screen_width<=0.0f||c->screen_height<=0.0f||c->focal<=0.0f||
       !isfinite(c->yaw)||!isfinite(c->pitch)||!isfinite(c->distance)||c->distance<=0.001f)return p;
    float px=x-b->center_x,py=y-b->center_y,pz=z-b->center_z;
    float sy=sinf(c->yaw),cy=cosf(c->yaw);
    float x1=px*cy-pz*sy;
    float z1=px*sy+pz*cy;
    float sp=sinf(c->pitch),cp=cosf(c->pitch);
    float y1=py*cp-z1*sp;
    float z2=py*sp+z1*cp+c->distance;
    p.depth=z2;
    if(!isfinite(z2)||z2<=0.001f)return p;
    float scale=c->focal/z2;
    p.x=c->screen_width*0.5f+x1*scale;
    p.y=c->screen_height*0.5f-y1*scale;
    p.visible=isfinite(p.x)&&isfinite(p.y);
    return p;
}
