#ifndef FP_SCENE_H
#define FP_SCENE_H
#include <stddef.h>

typedef struct {
    float min_x,min_y,min_z;
    float max_x,max_y,max_z;
    float center_x,center_y,center_z;
    float radius;
} TsFpSceneBounds;

typedef struct {
    float yaw;
    float pitch;
    float distance;
    float focal;
    float screen_width;
    float screen_height;
} TsFpSceneCamera;

typedef struct {
    float x,y,depth;
    int visible;
} TsFpScenePoint;

void tsfp_scene_bounds_reset(TsFpSceneBounds *b);
void tsfp_scene_bounds_add(TsFpSceneBounds *b,float x,float y,float z);
void tsfp_scene_camera_fit(TsFpSceneCamera *c,const TsFpSceneBounds *b,float width,float height);
TsFpScenePoint tsfp_scene_project(const TsFpSceneCamera *c,const TsFpSceneBounds *b,float x,float y,float z);

#endif
