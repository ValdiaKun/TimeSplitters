#include "fp_scene.h"
#include <assert.h>
#include <math.h>

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
    return 0;
}
