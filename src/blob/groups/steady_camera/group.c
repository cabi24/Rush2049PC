#include "game/game.h"

float fabsf(float); /* : m2c emits fabsf/sqrtf for abs.s/sqrt.s; declare them as IDO intrinsics */
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

typedef struct V3 { float x, y, z; } V3;
typedef struct Fx { unsigned char p0[0x60]; V3 n60; unsigned char p6c[0x18]; float v84[3]; unsigned char p90[8]; } Fx;

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

VIEW(V3[], steady_start_pos);
#define steady_start_pos steady_start_pos_view

VIEW(Fx[], gCamera);
#define gCamera gCamera_view

VIEW(V3[], cur_acc);
#define cur_acc cur_acc_view

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
    u8 pb0[0x46];
    s16 s_f6;
    u8 pf8[0x110 - 0xF8]; struct pc_visual visuals[21]; u8 pf8_308[0x35C - 0x308];
    s8 player;
    s8 mode;
    s8 mode2;
    u8 p35f[0x21];
    Q *q;
} Cr;

#define MAX_VEL 100.0f

/* steady_move_cam, 0x800E95DC..0x800E9C70 (1,684 bytes).
 * N64 per-player descendant of arcade camera.c:steady_move_cam.
 * Compile only in the genuine accepted UpdateRearCamera group (verify.py).
 * IDO 5.3 -g0 -O3 -mips2 -G 0 -non_shared, normal -r4300_mul.
 * The donor performs vecsub followed by scalmul. Preserve those two phases:
 * combining each component into one multiply expression changes scheduling.
 * Existing type/global declarations and accepted context bodies are unchanged.
 */
void steady_move_cam(s16 mode, Cr *car, f32 *pos, intptr_t uvs) {
    V3 delta[4];
    f32 res[3], pos2[3];
    s32 pl;

    pl = car->player;
    if (mode == 0) {
        steady_start_pos[pl].x = pos[0];
        steady_start_pos[pl].y = pos[1];
        steady_start_pos[pl].z = pos[2];
        gCamera[pl].v84[0] = pos[0];
        gCamera[pl].v84[1] = pos[1];
        gCamera[pl].v84[2] = pos[2];
        res[0] = car->pos[0];
        res[1] = car->pos[1];
        res[2] = car->pos[2];
        pos[0] = pos[0] - res[0];
        pos[1] = pos[1] - res[1];
        pos[2] = pos[2] - res[2];
        LookInDir(pos, &gCamera[pl].n60);
        steady_stage[pl] = 0;
        steady_leg_time[pl] = 0.0f;
        return;
    }
    switch (steady_stage[pl]) {
    case 0:
        UpdateRearCamera(car, rear_z[pl], rear_y[pl], res);
        res[0] = pos[0] + res[0];
        res[1] = pos[1] + res[1];
        res[2] = pos[2] + res[2];
        delta[pl].x = res[0] - steady_start_pos[pl].x;
        delta[pl].y = res[1] - steady_start_pos[pl].y;
        delta[pl].z = res[2] - steady_start_pos[pl].z;
        delta[pl].x = delta[pl].x * (.1f * steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        delta[pl].y = delta[pl].y * (.1f * steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        delta[pl].z = delta[pl].z * (.1f * steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        pos2[0] = car->pos[0] - car->m[6] * 40.0f;
        pos2[1] = car->pos[1] + 5.0f;
        pos2[2] = car->pos[2] - car->m[8] * 40.0f;
        delta[pl].x = pos2[0] - steady_start_pos[pl].x;
        delta[pl].y = pos2[1] - steady_start_pos[pl].y;
        delta[pl].z = pos2[2] - steady_start_pos[pl].z;
        delta[pl].x = delta[pl].x * (steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        delta[pl].y = delta[pl].y * (steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        delta[pl].z = delta[pl].z * (steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        break;
    case 1:
        UpdateRearCamera(car, rear_z[pl], rear_y[pl], res);
        res[0] = pos[0] + res[0];
        res[1] = pos[1] + res[1];
        res[2] = pos[2] + res[2];
        delta[pl].x = res[0] - steady_start_pos[pl].x;
        delta[pl].y = res[1] - steady_start_pos[pl].y;
        delta[pl].z = res[2] - steady_start_pos[pl].z;
        delta[pl].x = delta[pl].x * (steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        delta[pl].y = delta[pl].y * (steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        delta[pl].z = delta[pl].z * (steady_leg_time[pl] / steady_leg_duration[steady_stage[pl]]);
        break;
    case 2:
        init_view3(car);
        if (!(gGameState & 8)) {
            s32 v;
            if (car->q->h[0]->f2c != 0)
                v = GetPlayerCarView(car->q->h);
            else
                v = 2;
            car->mode = v;
            car->mode2 = v;
            if (car->mode == 1) {
                cur_acc[car->player].x = 0;
                cur_acc[car->player].y = 0;
                cur_acc[car->player].z = 0;
            }
        }
        break;
    }
    if (steady_stage[pl] == 0 || steady_stage[pl] == 1) {
        pos2[0] = steady_start_pos[pl].x + delta[pl].x;
        pos2[1] = steady_start_pos[pl].y + delta[pl].y;
        pos2[2] = steady_start_pos[pl].z + delta[pl].z;
        res[0] = pos2[0] - pos[0];
        res[1] = pos2[1] - pos[1];
        res[2] = pos2[2] - pos[2];
        elastic_factor[pl] = 0;
        point_at_car(car, pos, uvs, res);
        gCamera[pl].v84[1] += 4.0f;
        steady_leg_time[pl] += gFrameSeconds;
        if (steady_leg_duration[steady_stage[pl]] <= steady_leg_time[pl]) {
            steady_stage[pl]++;
            steady_leg_time[pl] = 0.0f;
            steady_start_pos[pl].x = gCamera[pl].v84[0];
            steady_start_pos[pl].y = gCamera[pl].v84[1];
            steady_start_pos[pl].z = gCamera[pl].v84[2];
        }
    }
}

