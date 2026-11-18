/* steady_move_cam: the steady camera (MAT3 camera.c): at a race's start the camera glides from in front of the
 * car round behind it and settles into the car's chosen view. */
#include "game/game.h"

float fabsf(float);
float sqrtf(float);

typedef struct V3 { float x, y, z; } V3;


/* A player's profile (SaveSlot, host layout: pc_prelude.h): its save data handle. */
typedef struct HN { void *next, *prev, *fileh; u8 p[0x2C - 0xC]; void *f2c; } HN;
/* A player's input record (gPlayers): its profile's handle. */
typedef struct Q { u8 p[0x48]; HN **h; } Q;

/* Car `car`'s steady camera (view 7). `mode` 0 starts it at `pos` looking at the car (init_steady_move_cam). Then
 * it flies in legs of steady_leg_duration seconds, pointing at the car (`pos`, its orientation into `uvs`): leg 0
 * from the start to 40 feet behind the car and 5 up, leg 1 from there to the rear chase camera's place
 * (UpdateRearCamera), and at the end (leg 2) the car's view becomes view3's, or a player's saved view (or 2). */
void steady_move_cam(s16 mode, tCarData *car, f32 *pos, void *uvs) {
    V3 delta[4];
    f32 res[3], pos2[3];
    s32 pl;

    pl = car->camera;
    if (mode == 0) {
        steady_start_pos[pl].x = pos[0];
        steady_start_pos[pl].y = pos[1];
        steady_start_pos[pl].z = pos[2];
        gCamera[pl].fcam.pos[0] = pos[0];
        gCamera[pl].fcam.pos[1] = pos[1];
        gCamera[pl].fcam.pos[2] = pos[2];
        res[0] = car->dr_pos[0];
        res[1] = car->dr_pos[1];
        res[2] = car->dr_pos[2];
        pos[0] = pos[0] - res[0];
        pos[1] = pos[1] - res[1];
        pos[2] = pos[2] - res[2];
        LookInDir(pos, gCamera[pl].fcam.uvs);
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
        /* Original game: the step towards the chase camera above is overwritten at once; leg 0 goes only to the
         * point behind the car. */
        pos2[0] = car->dr_pos[0] - car->dr_uvs[2][0] * 40.0f;
        pos2[1] = car->dr_pos[1] + 5.0f;
        pos2[2] = car->dr_pos[2] - car->dr_uvs[2][2] * 40.0f;
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
        if (!(gGameState & GS_DEMO)) {
            s32 v;
            if (((Q *)car->player)->h[0]->f2c != 0)
                v = GetPlayerCarView(((Q *)car->player)->h);
            else
                v = 2;
            car->view = v;
            car->saveview = v;
            if (car->view == 1) {
                cur_acc[car->camera][0] = 0;
                cur_acc[car->camera][1] = 0;
                cur_acc[car->camera][2] = 0;
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
        gCamera[pl].fcam.pos[1] += 4.0f;
        steady_leg_time[pl] += gFrameSeconds;
        if (steady_leg_duration[steady_stage[pl]] <= steady_leg_time[pl]) {
            steady_stage[pl]++;
            steady_leg_time[pl] = 0.0f;
            steady_start_pos[pl].x = gCamera[pl].fcam.pos[0];
            steady_start_pos[pl].y = gCamera[pl].fcam.pos[1];
            steady_start_pos[pl].z = gCamera[pl].fcam.pos[2];
        }
    }
}

