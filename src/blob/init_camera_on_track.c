/* init_camera_on_track: a camera fixed beside the track ahead of the car (MAT3 camera.c). */
#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its speed and its place on its drone path. */
typedef struct Object { u8 other0[248]; s16 mph; u8 other250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 other250_308[0x338 - 0x308]; s16 idx; s16 other826; s16 path; } Object;
VIEW(PcRespawn *[], trackSpots);
#define trackSpots trackSpots_view

/* Sets car `object`'s track camera (view 6, fix_camera_in_space) on drone path 0 a little ahead of the car (5
 * points, more the faster it goes): at the track's first camera spot within 200 feet of there (trackSpots, which
 * says if it zooms), else 10 to 20 feet above the path point, kept out of the walls. */
void init_camera_on_track(Object *object)
{
    s16 i;
    s16 point;
    PcRespawn *e;
    Vec3 delta;
    Vec3 pos;
    Mat3 matrix;

    i = (s16)(object->mph >> 6) + 4;
    point = object->idx;
    /* Original game: the point is stepped along drone path 0 but read on the car's own drone path; MAT3 has it so
     * too. */
    for (; i >= 0; i--) point = NextPathPt(point, 0);
    pos[0] = gDronePaths[object->path].pts[point].pos[0];
    pos[1] = gDronePaths[object->path].pts[point].pos[1];
    pos[2] = gDronePaths[object->path].pts[point].pos[2];
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
