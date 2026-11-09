#include "game/game.h"

float fabsf(float); /* : m2c emits fabsf/sqrtf for abs.s/sqrt.s; declare them as IDO intrinsics */
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

typedef struct V3 { float x, y, z; } V3;
typedef struct Fx { unsigned char p0[0x60]; V3 n60; unsigned char p6c[0x18]; float v84[3]; unsigned char p90[8]; } Fx;

VIEW(Fx[], gCamera);
#define gCamera gCamera_view

void point_at_car(void *arg0, intptr_t arg1, intptr_t arg2, f32 *arg3);

/*
 * group UpdateRearCamera (supersedes src/blob/groups/rear_camera, the cloud A seed).
 * Strict members: UpdateRearCamera, DeathCam, UpdateCarObj (EA2DC unchanged from the locked seed
 * except that its two calls use the corrected parameter order). Context: steady_move_cam (2 words off).
 *
 * UpdateRearCamera = arcade game/camera.c update_rear_camera(ab, AB, camoff) with the car record added as
 *   the first parameter: (car, ab, AB, camoff). Internal (IPA register parameters s0=car, f22=ab,
 *   f24=AB, s4=camoff; clobbers s1-s6/f20-f30 unsaved). Arcade body pasted; N64 changes: fixed_cam
 *   is car->mode == 8, the death-cam branch is mode 4 (vec = {V[1], V[0], 0}), the "abrupt
 *   movement" timer is gone, the fabs is applied to vec[2] / res[1] (Y-up axes), and the elastic
 *   factor goes to the per-slot elastic_factor[car->player].
 *   Quirks: the call-site argument order (car, ab, AB, camoff) fixes the s0/f22/f24/s4 set-up order
 *   in every caller (it was the last residual of E95DC's case bodies); 0.0f float zeros for vec/elastic
 *   but int 0 for magvel = 0; int 1 for "magvel > 1" and the interpolate lo, 1.0f in "1.0f - x";
 *   a dead 8-byte local (char buf[8]; arcade has char buf[60] for debug text) gives the 160-byte frame;
 *   unused arcade locals (uvs[3][3], fixed_cam, flag) kept.
 * DeathCam = arcade DeathCam (N64: update_rear_camera(car, 30, 16, res), elasticity .13 in
 *   the per-slot elasticity, no carpos copy). Arcade loop expressions verbatim.
 * steady_move_cam = arcade steady_move_cam, rebuilt for N64 per-player legs.
 */
/* group members */
typedef struct HN { void *next, *prev, *fileh; u8 p[0x2C - 0xC]; void *f2c; } HN;   /* the profile, pc_prelude.h */
typedef struct Q { u8 p[0x48]; HN **h; } Q;
typedef struct Cr {
    u8 p0[8];
    f32 pos[3];
    f32 vx, vy, vz;
    u8 p20[0xC];
    f32 m[9];
    u8 p50[0x58];
    f32 f_a8, f_ac;
    /* PC: +0xF4 is the car's scene-node handle, an s32; s_f6 is its low half, the second s16 on the N64 and the
     * first on a little-endian host (docs/port/ENDIAN.md 3.1). */
    u8 pb0[0x44];
    s16 s_f6;
    s16 s_f6_high;
    u8 pf8[0x110 - 0xF8]; struct pc_visual visuals[21]; u8 pf8_308[0x35C - 0x308];
    s8 player;
    s8 mode;
    s8 mode2;
    u8 p35f[0x21];
    Q *q;
} Cr;

#define MAX_VEL 100.0f
/* game.frame_rate 60: DeathCam moves the camera a gain g of the way to its target each frame of two retraces; a
 * frame of one keeps (1 - g)^(1/2) of the way (pc_game_fps.h, docs/port/FRAMERATE.md 7). */
#define PC_GAIN(g) (pc_fps60 ? 1.0f - pc_fps_filter(1.0f - (g)) : (g))

void UpdateRearCamera(Cr *car, f32 ab, f32 AB, f32 *camoff) {
    f32 vec[3], res[3], temp[3], A, a, B, b, fact, magvel, uvs[3][3];
    s16 fixed_cam, flag;
    char buf[8];

    s32 pl;

    pl = car->player;
    if (car->mode == 8)
        magvel = 0;
    else
        magvel = sqrtf(car->vx*car->vx + car->vz*car->vz);

    if (magvel <= MAX_VEL) {
        vec[0] = 0.0f;
        vec[1] = AB;
        vec[2] = -ab;
        if (car->mode == 8) {
            temp[0] = vec[0];
            temp[1] = vec[1];
            temp[2] = vec[2];
        } else
            WorldVector(vec, temp, car->m);
        temp[1] = fabsf(temp[1]);
    }

    if (magvel > 1) {
        if (car->mode == 4) {
            vec[0] = car->f_ac;
            vec[1] = car->f_a8;
            vec[2] = 0.0f;
            WorldVector(vec, res, car->m);
            res[0] = -res[0];
            res[2] = -res[2];
        } else {
            res[0] = car->vx;
            res[1] = car->vy;
            res[2] = car->vz;
            if (ab < 0)
                res[1] = -res[1];
            BodyVector(res, vec, car->m);
            vec[2] = fabsf(vec[2]);
            WorldVector(vec, res, car->m);
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

    if (car->mode == 8)
        elastic_factor[pl] = 0.0f;
    else if (car->mode == 4)
        elastic_factor[pl] = 1.0f - magvel / MAX_VEL;
    else if (magvel < MAX_VEL)
        elastic_factor[pl] = 1.0f - magvel * .01f;
    else
        elastic_factor[pl] = 0.0f;

    camoff[0] = res[0];
    camoff[1] = res[1];
    camoff[2] = res[2];
}

void DeathCam(Cr *car, f32 *pos, f32 *uvs) {
    s32 i, j;
    f32 rpos[3], res[3];
    s32 slot;

    slot = car->player;
    ZOID_UpdateObject(car->s_f6, pos, uvs);
    elasticity[slot] = PC_GAIN(.13f);
    UpdateRearCamera(car, 30, 16, res);
    for (i = 0; i < 3; i++)
        gCamera[slot].v84[i] = (gCamera[slot].v84[i]*(1.0f-elasticity[slot]) + (pos[i]+res[i])*elasticity[slot]);
    for (i = 0; i < 3; i++)
        rpos[i] = pos[i] - gCamera[slot].v84[i];
    LookInDir(rpos, &gCamera[slot].n60);
}

void UpdateCarObj(Cr *car, void *arg1, void *arg2, intptr_t arg3) {
    volatile s32 padv[5];
    f32 v[3];
    s32 pl;

    pl = car->player;
    update_car_object(car, arg1, arg2);
    if (car->mode == 0xA) {
        UpdateRearCamera(car, -rear_z[pl], rear_y[pl], v);
    } else {
        UpdateRearCamera(car, rear_z[pl], rear_y[pl], v);
    }
    point_at_car(car, arg3, 0, v);
}

