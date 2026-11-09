/* The view3 chase camera (MAT3 camera.c, resurrect.c's Interpolate): a camera behind and above the car that swings
 * between the car's heading and its direction of travel, and between the car's up and the world's, by how the car
 * is going; and update_car_object, the car's scene object to its dead-reckoned place. */
#include "game/game.h"

float sqrtf(float);

VIEW(tCamera[], gCamera);
#define gCamera gCamera_view
VIEW(f32[3], uvector);
#define uvector uvector_view
VIEW(f32[3], rvector);
#define rvector rvector_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* The car record (MAT3 tCarData; the PC's pc_car, host layout: its visuals are 0x20 bytes). */
struct tCarData {
    u8 pad0[0x14];
    f32 dr_vel[3];          /* 0x14 its velocity */
    u8 pad20[0x2C - 0x20];
    f32 dr_uvs[3][3];       /* 0x2C its orientation */
    f32 dr_car_uvs[3][3];   /* 0x50 the orientation it is drawn with */
    u8 pad74[0xA4 - 0x74];
    f32 v[3];               /* 0xA4 its velocity in its own frame */
    u8 padB0[0xF4 - 0xB0];
    s32 objnum;             /* 0xF4 its scene object, -1 none */
    u8 padF8[0x110 - 0xF8]; struct pc_visual visuals[21]; u8 pad308[0x35C - 0x308];
    s8 camera;              /* 0x35C its camera */
    s8 view;                /* 0x35D its camera's view */
};

/* Sets `out` (`n` floats) to the point between `a` and `b` that `t` is between `lo` and `hi` (a if lo is hi). */
void Interpolate(f32 *a, f32 *b, s16 n, f32 lo, f32 hi, f32 t, f32 *out) {
    f32 k;
    s16 i;
    k = hi - lo;
    if (hi != lo) {
        k = (t - lo) / k;
    }
    for (i = 0; i < n; i++) {
        out[i] = (b[i] - a[i]) * k + a[i];
    }
}
/* Moves car `car`'s scene object to position `pos` and orientation `uvs`, its up axis squashed by the frame scale
 * (FSCLS_from_one: the N64's display aspect). */
void update_car_object(struct tCarData *car, f32 *pos, void *uvs) {
    f32 m[9];
    CopyUV(uvs, m);
    if (gFrameScale != 0) {
        m[1] *= FSCLS_from_one[gFrameScale];
        m[4] *= FSCLS_from_one[gFrameScale];
        m[7] *= FSCLS_from_one[gFrameScale];
    }
    if (car->objnum >= 0) {
        ZOID_UpdateObject(car->objnum, pos, m);
    }
}
/* Sets the view3 camera of car `car` (views 2 and 3, 3 nearer and lower) behind `pos`: rear_z back and rear_y up,
 * stretched with speed; its back direction (zvec) swings from the direction of travel (zf 0) to the car's back
 * (zf 1), towards the back with the car on the ground or slow, its up direction (yvec) from the world's up (yf 0)
 * to the car's (yf 1), towards the car's when its suspension is loaded; both lag last frame's by an elasticity
 * that falls with speed (none just after a switch, or in battle). The camera is built in fcam, looking at the car. */
void View3Cam(struct tCarData *car, f32 *pos) {
    s32 camera;
    f32 len;
    f32 rear_dist;
    f32 up_dist;
    s32 i;
    s32 unused;
    f32 rad;
    f32 stretch;
    f32 bvec[3];
    f32 fvec[3];
    f32 zvec[3];
    f32 yvec[3];
    f32 camoff[3];
    f32 elast;

    camera = car->camera;
    rear_dist = rear_z[camera];
    up_dist = rear_y[camera];
    update_car_object(car, pos, car->dr_car_uvs);
    if (car->view == 3) {
        rear_dist -= 6.0f;
        up_dist -= 2.0f;
    }
    len = vector_magnitude(car->dr_vel);
    if (len > 100.0f) {
        stretch = 0.25f;
    } else {
        stretch = len * view3_speed_stretch;
    }
    rad = sqrtf(rear_dist * rear_dist + up_dist * up_dist) * (1.0f + stretch);
    rear_dist *= 1.0f + stretch;
    if (view3_switch[camera] != 0 || gRace == kBattle) {
        elast = 0.0;
    } else if (len < 140.0f) {
        elast = PC_FILTER(view3_elastic_max - (len / 140.0f) * view3_elastic_range);
    } else {
        elast = PC_FILTER(view3_elastic_min);
    }
    view3_switch[camera] = 0;
    if (gTireCnt[camera] >= 3 && gSusAvg[camera] > 0.5f) {
        zf[camera] = zf[camera] + gFrameSeconds;
    } else if (len < 30.0f) {
        zf[camera] += gFrameSeconds * 10.0f;
    } else {
        zf[camera] = zf[camera] - gFrameSeconds;
    }
    if (zf[camera] < 0.0f) {
        zf[camera] = 0.0f;
    } else if (zf[camera] > 1.0f) {
        zf[camera] = 1.0f;
    }
    if (gTireCnt[camera] < 3) {
        yf[camera] -= gFrameSeconds;
    } else {
        if (gSusAvg[camera] > 1.5f) {
            yf[camera] += gFrameSeconds * 10.0f;
        } else if (gSusAvg[camera] > 0.5f) {
            yf[camera] += gFrameSeconds / (view3_yf_rate_base + (1.5f - gSusAvg[camera]) * view3_yf_rate_range);
        } else {
            yf[camera] += gFrameSeconds;
        }
    }
    if (yf[camera] < 0.0f) {
        yf[camera] = 0.0f;
    } else if (yf[camera] > 1.0f) {
        yf[camera] = 1.0f;
    }
    for (i = 0; i < 3; i++) {
        bvec[i] = -car->dr_uvs[2][i];
    }
    if (zf[camera] < 1.0f) {
        if (len < 1.0f) {
            fvec[0] = bvec[0];
            fvec[1] = bvec[1];
            fvec[2] = bvec[2];
        } else if (car->v[2] < 0 && gTireCnt[camera] >= 3) {
            fvec[0] = car->dr_vel[0] * (1 / len);
            fvec[1] = car->dr_vel[1] * (1 / len);
            fvec[2] = car->dr_vel[2] * (1 / len);
        } else {
            fvec[0] = car->dr_vel[0] * (-1 / len);
            fvec[1] = car->dr_vel[1] * (-1 / len);
            fvec[2] = car->dr_vel[2] * (-1 / len);
        }
    }
    if (zf[camera] == 0.0f) {
        zvec[0] = fvec[0]; zvec[1] = fvec[1]; zvec[2] = fvec[2];
    } else if (zf[camera] == 1.0f) {
        zvec[0] = bvec[0]; zvec[1] = bvec[1]; zvec[2] = bvec[2];
    } else {
        Interpolate(fvec, bvec, 3, 0.0f, 1.0f, zf[camera], zvec);
        len = vector_magnitude(zvec);
        if (len < view3_zvec_min_len) {
            zvec[0] = bvec[0]; zvec[1] = bvec[1]; zvec[2] = bvec[2];
        } else {
            zvec[0] *= (1.0f / len); zvec[1] *= (1.0f / len); zvec[2] *= (1.0f / len);
        }
    }
    if (elast > 0) {
        zvec[0] *= (1.0f - elast); zvec[1] *= (1.0f - elast); zvec[2] *= (1.0f - elast);
        zvec[0] += last_zvec[camera][0] * elast;
        zvec[1] += last_zvec[camera][1] * elast;
        zvec[2] += last_zvec[camera][2] * elast;
        len = vector_magnitude(zvec);
        if (len < view3_zvec_elastic_min_len) {
            zvec[0] = bvec[0]; zvec[1] = bvec[1]; zvec[2] = bvec[2];
        } else {
            zvec[0] *= (1.0f / len); zvec[1] *= (1.0f / len); zvec[2] *= (1.0f / len);
        }
    }
    last_zvec[camera][0] = zvec[0];
    last_zvec[camera][1] = zvec[1];
    last_zvec[camera][2] = zvec[2];
    for (i = 0; i < 3; i++) {
        bvec[i] = car->dr_uvs[1][i];
    }
    if (yf[camera] < 1.0f) {
        fvec[0] = uvector[0];
        fvec[1] = uvector[1];
        fvec[2] = uvector[2];
    }
    if (yf[camera] == 0) {
        yvec[0] = fvec[0]; yvec[1] = fvec[1]; yvec[2] = fvec[2];
    } else if (yf[camera] == 1.0f) {
        yvec[0] = bvec[0]; yvec[1] = bvec[1]; yvec[2] = bvec[2];
    } else {
        Interpolate(fvec, bvec, 3, 0, 1.0f, yf[camera], yvec);
        len = vector_magnitude(yvec);
        if (len < view3_yvec_min_len) {
            yvec[0] = bvec[0]; yvec[1] = bvec[1]; yvec[2] = bvec[2];
        } else {
            yvec[0] *= (1.0f / len); yvec[1] *= (1.0f / len); yvec[2] *= (1.0f / len);
        }
    }
    if (elast > 0) {
        yvec[0] *= (1.0f - elast); yvec[1] *= (1.0f - elast); yvec[2] *= (1.0f - elast);
        yvec[0] += last_yvec[camera][0] * elast;
        yvec[1] += last_yvec[camera][1] * elast;
        yvec[2] += last_yvec[camera][2] * elast;
        len = vector_magnitude(yvec);
        if (len < view3_yvec_elastic_min_len) {
            yvec[0] = bvec[0]; yvec[1] = bvec[1]; yvec[2] = bvec[2];
        } else {
            yvec[0] *= (1.0f / len); yvec[1] *= (1.0f / len); yvec[2] *= (1.0f / len);
        }
    }
    last_yvec[camera][0] = yvec[0];
    last_yvec[camera][1] = yvec[1];
    last_yvec[camera][2] = yvec[2];
    camoff[0] = zvec[0] * rear_dist; camoff[1] = zvec[1] * rear_dist; camoff[2] = zvec[2] * rear_dist;
    camoff[0] += yvec[0] * up_dist;
    camoff[1] += yvec[1] * up_dist;
    camoff[2] += yvec[2] * up_dist;
    vector_direction(camoff, gCamera[camera].fcam.uvs[2]);
    camoff[0] = gCamera[camera].fcam.uvs[2][0] * rad;
    camoff[1] = gCamera[camera].fcam.uvs[2][1] * rad;
    camoff[2] = gCamera[camera].fcam.uvs[2][2] * rad;
    if (car->view == 2) {
        camoff[0] += yvec[0] * 4.0f;
        camoff[1] += yvec[1] * 4.0f;
        camoff[2] += yvec[2] * 4.0f;
    } else if (car->view == 3) {
        camoff[0] += yvec[0] * 4.0f;
        camoff[1] += yvec[1] * 4.0f;
        camoff[2] += yvec[2] * 4.0f;
    }
    gCamera[camera].fcam.pos[0] = pos[0] + camoff[0];
    gCamera[camera].fcam.pos[1] = pos[1] + camoff[1];
    gCamera[camera].fcam.pos[2] = pos[2] + camoff[2];
    gCamera[camera].fcam.uvs[2][0] *= -1.0f;
    gCamera[camera].fcam.uvs[2][1] *= -1.0f;
    gCamera[camera].fcam.uvs[2][2] *= -1.0f;
    gCamera[camera].fcam.uvs[0][0] = yvec[1] * gCamera[camera].fcam.uvs[2][2] - yvec[2] * gCamera[camera].fcam.uvs[2][1];
    gCamera[camera].fcam.uvs[0][1] = yvec[2] * gCamera[camera].fcam.uvs[2][0] - yvec[0] * gCamera[camera].fcam.uvs[2][2];
    gCamera[camera].fcam.uvs[0][2] = yvec[0] * gCamera[camera].fcam.uvs[2][1] - yvec[1] * gCamera[camera].fcam.uvs[2][0];
    len = vector_magnitude(gCamera[camera].fcam.uvs[0]);
    if (len < view3_xvec_min_len) {
        gCamera[camera].fcam.uvs[0][0] = rvector[0];
        gCamera[camera].fcam.uvs[0][1] = rvector[1];
        gCamera[camera].fcam.uvs[0][2] = rvector[2];
    } else {
        gCamera[camera].fcam.uvs[0][0] *= (1.0f / len);
        gCamera[camera].fcam.uvs[0][1] *= (1.0f / len);
        gCamera[camera].fcam.uvs[0][2] *= (1.0f / len);
    }
    gCamera[camera].fcam.uvs[1][0] = gCamera[camera].fcam.uvs[2][1] * gCamera[camera].fcam.uvs[0][2] - gCamera[camera].fcam.uvs[2][2] * gCamera[camera].fcam.uvs[0][1];
    gCamera[camera].fcam.uvs[1][1] = gCamera[camera].fcam.uvs[2][2] * gCamera[camera].fcam.uvs[0][0] - gCamera[camera].fcam.uvs[2][0] * gCamera[camera].fcam.uvs[0][2];
    gCamera[camera].fcam.uvs[1][2] = gCamera[camera].fcam.uvs[2][0] * gCamera[camera].fcam.uvs[0][1] - gCamera[camera].fcam.uvs[2][1] * gCamera[camera].fcam.uvs[0][0];
}

