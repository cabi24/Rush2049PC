/* The chase and crash cameras (MAT3 camera.c): UpdateRearCamera, the camera behind the car
 * that the steady and crash views build on, DeathCam (the crash view, 4) and UpdateCarObj (the demo's car camera,
 * views 8 and 10). */
#include "game/game.h"

float fabsf(float);
float sqrtf(float);

typedef struct V3 { float x, y, z; } V3;



#define MAX_VEL 100.0f
/* game.frame_rate 60: DeathCam moves the camera a gain g of the way to its target each frame of two retraces; a
 * frame of one keeps (1 - g)^(1/2) of the way (pc_game_fps.h, docs/port/FRAMERATE.md 7). */
#define PC_GAIN(g) (pc_fps60 ? 1.0f - pc_fps_filter(1.0f - (g)) : (g))

/* Sets `camoff` to where the chase camera sits from car `car`: `ab` back and `AB` up (a negative `ab` in front).
 * Slow (below 100 feet a second), between behind the car and behind its direction of travel by speed; fast, behind
 * its direction of travel, kept above the ground. The crash view (4) follows the car's own motion instead; the
 * fixed-camera view (8) ignores the car's speed and orientation. Its elasticity (elastic_factor), how much the
 * camera lags, falls with speed. The char buffer is the N64 code's (it never uses it). */
void UpdateRearCamera(tCarData *car, f32 ab, f32 AB, f32 *camoff) {
    f32 vec[3], res[3], temp[3], A, a, B, b, fact, magvel, uvs[3][3];
    s16 fixed_cam, flag;
    char buf[8];

    s32 pl;

    pl = car->camera;
    if (car->view == 8)
        magvel = 0;
    else
        magvel = sqrtf(car->dr_vel[0]*car->dr_vel[0] + car->dr_vel[2]*car->dr_vel[2]);

    if (magvel <= MAX_VEL) {
        vec[0] = 0.0f;
        vec[1] = AB;
        vec[2] = -ab;
        if (car->view == 8) {
            temp[0] = vec[0];
            temp[1] = vec[1];
            temp[2] = vec[2];
        } else
            WorldVector(vec, temp, car->dr_uvs[0]);
        temp[1] = fabsf(temp[1]);
    }

    if (magvel > 1) {
        if (car->view == 4) {
            vec[0] = car->v[2];
            vec[1] = car->v[1];
            vec[2] = 0.0f;
            WorldVector(vec, res, car->dr_uvs[0]);
            res[0] = -res[0];
            res[2] = -res[2];
        } else {
            res[0] = car->dr_vel[0];
            res[1] = car->dr_vel[1];
            res[2] = car->dr_vel[2];
            if (ab < 0)
                res[1] = -res[1];
            BodyVector(res, vec, car->dr_uvs[0]);
            vec[2] = fabsf(vec[2]);
            WorldVector(vec, res, car->dr_uvs[0]);
            res[0] = -res[0];
            res[2] = -res[2];
        }

        fact = ab / magvel;
        res[0] *= fact;
        res[1] *= fact;
        res[2] *= fact;

        a = res[1];
        b = sqrtf(res[0]*res[0] + res[2]*res[2]);

        if (b > .001f) {
            A = -a * AB / ab;
            fact = (b - A) / b;
            res[0] *= fact;
            res[2] *= fact;
        }

        B = b * AB / ab;
        res[1] = fabsf(B - res[1]);

        if (magvel < MAX_VEL)
            Interpolate(temp, res, 3, 1, MAX_VEL, magvel, res);
    } else {
        res[0] = temp[0];
        res[1] = temp[1];
        res[2] = temp[2];
    }

    if (car->view == 8)
        elastic_factor[pl] = 0.0f;
    else if (car->view == 4)
        elastic_factor[pl] = 1.0f - magvel / MAX_VEL;
    else if (magvel < MAX_VEL)
        elastic_factor[pl] = 1.0f - magvel * .01f;
    else
        elastic_factor[pl] = 0.0f;

    camoff[0] = res[0];
    camoff[1] = res[1];
    camoff[2] = res[2];
}

/* The crash view (4) of car `car`, at `pos` with orientation `uvs`: its scene object moved there, the camera eases
 * (13% a frame of two retraces) towards a place 30 feet back and 16 up and looks at the car. */
void DeathCam(tCarData *car, f32 *pos, f32 *uvs) {
    s32 i, j;
    f32 rpos[3], res[3];
    s32 slot;

    slot = car->camera;
    ZOID_UpdateObject(car->objnum, pos, uvs);
    elasticity[slot] = PC_GAIN(.13f);
    UpdateRearCamera(car, 30, 16, res);
    for (i = 0; i < 3; i++)
        gCamera[slot].fcam.pos[i] = (gCamera[slot].fcam.pos[i]*(1.0f-elasticity[slot]) + (pos[i]+res[i])*elasticity[slot]);
    for (i = 0; i < 3; i++)
        rpos[i] = pos[i] - gCamera[slot].fcam.pos[i];
    LookInDir(rpos, gCamera[slot].fcam.uvs);
}

/* The demo's car cameras (views 8 and 10) of car `car`: its scene object moved to `pos` with orientation `uvs`,
 * the camera at the rear chase place (in front of the car in view 10) and pointing at `lookat`. */
void UpdateCarObj(tCarData *car, void *pos, void *uvs, f32 *lookat) {
    volatile s32 padv[5];
    f32 v[3];
    s32 pl;

    pl = car->camera;
    update_car_object(car, pos, uvs);
    if (car->view == 0xA) {
        UpdateRearCamera(car, -rear_z[pl], rear_y[pl], v);
    } else {
        UpdateRearCamera(car, rear_z[pl], rear_y[pl], v);
    }
    point_at_car(car, lookat, 0, v);
}

