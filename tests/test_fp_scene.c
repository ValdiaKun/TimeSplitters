#include "fp_scene.h"
#include <assert.h>
#include <math.h>
#include <float.h>

int main(void){
    TsFpSceneBounds b; tsfp_scene_bounds_reset(&b);
    tsfp_scene_bounds_add(&b,-2.0f,-1.0f,-3.0f);
    tsfp_scene_bounds_add(&b, 2.0f, 3.0f, 5.0f);
    assert(fabsf(b.center_x)<0.001f);
    assert(fabsf(b.center_y-1.0f)<0.001f);
    assert(fabsf(b.center_z-1.0f)<0.001f);
    assert(b.radius>0.0f);

    /* Invalid points must not poison valid bounds. */
    float min_x=b.min_x, radius=b.radius;
    tsfp_scene_bounds_add(&b,NAN,0.0f,0.0f);
    tsfp_scene_bounds_add(&b,0.0f,INFINITY,0.0f);
    tsfp_scene_bounds_add(&b,0.0f,0.0f,-INFINITY);
    assert(b.min_x==min_x && b.radius==radius);
    tsfp_scene_bounds_add(NULL,1.0f,2.0f,3.0f);

    TsFpSceneCamera c;
    tsfp_scene_camera_fit(&c,&b,800.0f,480.0f);
    TsFpScenePoint p=tsfp_scene_project(&c,&b,b.center_x,b.center_y,b.center_z);
    assert(p.visible);
    assert(fabsf(p.x-400.0f)<0.001f);
    assert(fabsf(p.y-240.0f)<0.001f);
    assert(p.depth>0.0f);

    c.yaw=0.7f;c.pitch=-0.35f;
    p=tsfp_scene_project(&c,&b,-2.0f,-1.0f,-3.0f);
    assert(p.visible && isfinite(p.depth) && p.depth>0.0f);

    /* Non-finite world coordinates and malformed camera state are invisible. */
    p=tsfp_scene_project(&c,&b,NAN,0.0f,0.0f);
    assert(!p.visible);
    p=tsfp_scene_project(&c,&b,INFINITY,0.0f,0.0f);
    assert(!p.visible);
    c.yaw=NAN;
    p=tsfp_scene_project(&c,&b,0.0f,0.0f,0.0f);
    assert(!p.visible);

    /* An invalid viewport must not produce apparently visible screen points. */
    tsfp_scene_camera_fit(&c,&b,0.0f,480.0f);
    p=tsfp_scene_project(&c,&b,b.center_x,b.center_y,b.center_z);
    assert(!p.visible);
    tsfp_scene_camera_fit(&c,&b,NAN,480.0f);
    p=tsfp_scene_project(&c,&b,b.center_x,b.center_y,b.center_z);
    assert(!p.visible);
    c.screen_width=800.0f;c.screen_height=480.0f;c.focal=240.0f;c.distance=0.0f;
    p=tsfp_scene_project(&c,&b,b.center_x+1.0f,b.center_y,b.center_z);
    assert(!p.visible);
    c.distance=-1.0f;
    p=tsfp_scene_project(&c,&b,b.center_x+1.0f,b.center_y,b.center_z);
    assert(!p.visible);
    c.distance=1.0f;c.focal=0.0f;
    p=tsfp_scene_project(&c,&b,b.center_x+1.0f,b.center_y,b.center_z);
    assert(!p.visible);

    /* Geometry behind the camera must be rejected by the near-plane check. */
    tsfp_scene_camera_fit(&c,&b,800.0f,480.0f);
    c.yaw=0.0f;c.pitch=0.0f;c.distance=1.0f;
    p=tsfp_scene_project(&c,&b,b.center_x,b.center_y,b.center_z-2.0f);
    assert(!p.visible);

    /* A single-point scene is degenerate but still has a stable camera fit. */
    TsFpSceneBounds point; tsfp_scene_bounds_reset(&point);
    tsfp_scene_bounds_add(&point,4.0f,5.0f,6.0f);
    assert(point.center_x==4.0f && point.center_y==5.0f && point.center_z==6.0f);
    assert(point.radius==0.0f);
    tsfp_scene_camera_fit(&c,&point,320.0f,240.0f);
    p=tsfp_scene_project(&c,&point,4.0f,5.0f,6.0f);
    assert(p.visible && fabsf(p.x-160.0f)<0.001f && fabsf(p.y-120.0f)<0.001f);

    /* Finite coordinates near FLT_MAX must not overflow the bounds center. */
    TsFpSceneBounds extreme; tsfp_scene_bounds_reset(&extreme);
    tsfp_scene_bounds_add(&extreme,FLT_MAX*0.75f,0.0f,0.0f);
    tsfp_scene_bounds_add(&extreme,FLT_MAX*0.90f,0.0f,0.0f);
    assert(isfinite(extreme.center_x) && extreme.center_x>0.0f);
    assert(isfinite(extreme.radius) && extreme.radius>0.0f);
    tsfp_scene_camera_fit(&c,&extreme,960.0f,544.0f);
    assert(isfinite(c.distance) && c.distance>0.0f);

    /* Large finite horizontal offsets can overflow float rotation even when
       the final perspective-divided screen coordinate is representable. */
    TsFpSceneBounds origin; tsfp_scene_bounds_reset(&origin);
    tsfp_scene_bounds_add(&origin,0.0f,0.0f,0.0f);
    c.yaw=0.78539816339f;c.pitch=0.0f;c.distance=FLT_MAX*0.99f;
    c.focal=240.0f;c.screen_width=800.0f;c.screen_height=480.0f;
    p=tsfp_scene_project(&c,&origin,FLT_MAX,0.0f,-FLT_MAX);
    assert(p.visible && isfinite(p.x) && isfinite(p.y) && isfinite(p.depth));

    /* Reject a perspective result that cannot be represented by the API's
       float screen coordinates, and reject corrupt scene centers up front. */
    c.yaw=0.0f;c.distance=0.01f;c.focal=FLT_MAX;
    p=tsfp_scene_project(&c,&origin,1.0f,0.0f,0.0f);
    assert(!p.visible && isfinite(p.depth) && p.depth>0.0f);
    origin.center_x=NAN;
    p=tsfp_scene_project(&c,&origin,0.0f,0.0f,0.0f);
    assert(!p.visible);
    return 0;
}
