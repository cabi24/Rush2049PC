#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
typedef struct Object { u8 other0[248]; s16 speed; u8 other250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 other250_308[0x338 - 0x308]; s16 point; s16 other826; s16 track; } Object;
typedef struct PathPoint { s16 x, y, z, pad; } PathPoint;
typedef struct { u16 count, other2; PathPoint *points; } Path;
typedef struct Respawn { s8 flag; u8 pad1[3]; Vec3 pos; } Respawn;
VIEW(Path[], gDronePaths);
#define gDronePaths gDronePaths_view
VIEW(Respawn *[], trackSpots);
#define trackSpots trackSpots_view

extern s32 CameraCollision(void *arg0, void *arg1);

void init_camera_on_track(Object *object)
{
    s16 i;
    s16 point;
    Respawn *e;
    Vec3 delta;
    Vec3 pos;
    Mat3 matrix;

    i = (s16)(object->speed >> 6) + 4;
    point = object->point;
    for (; i >= 0; i--) point = NextPathPt(point, 0);
    pos[0] = gDronePaths[object->track].points[point].x;
    pos[1] = gDronePaths[object->track].points[point].y;
    pos[2] = gDronePaths[object->track].points[point].z;
    e = trackSpots[gTrackNum];
    if (e != 0) {
        for (; e->pos[1] != -999.0f; e++) {
            delta[0] = e->pos[0] - pos[0];
            delta[1] = e->pos[1] - pos[1];
            delta[2] = e->pos[2] - pos[2];
            if (Distance2D(delta[0], delta[2]) > 200.0f) continue;
            if (e->flag) {
                fix_camera_in_space(1, object, e->pos, &matrix);
            } else {
                fix_camera_in_space(0, object, e->pos, &matrix);
            }
            return;
        }
    }
    delta[0] = pos[0];
    delta[1] = pos[1] + 10.0f + Random(10.0f);
    delta[2] = pos[2];
    CameraCollision(delta, pos);
    fix_camera_in_space(0, object, delta, &matrix);
}
