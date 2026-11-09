#include "game/game.h"

typedef struct View {
/* PC: a window is 80 bytes: the two words at 0 are pointers (ZOID_InitializeWindow's first and bounds;
 * docs/port/POINTERS.md 10). */
    /* 0x00 */ void *unk0;
    /* 0x04 */ void *unk4;
    /* 0x08 */ s32 ortho;
    /* 0x0C */ f32 fovx;
    /* 0x10 */ f32 fovy;
    /* 0x14 */ f32 tanx;
    /* 0x18 */ f32 tany;
    /* 0x1C */ f32 invx;
    /* 0x20 */ f32 invy;
    /* 0x24 */ f32 width;
    /* 0x28 */ f32 height;
    /* 0x2C */ f32 nearz;
    /* 0x30 */ f32 farz;
    /* 0x34 */ f32 aspect;
    /* 0x38 */ s32 pad[4];
} View;

VIEW(View[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

View *ZOID_SetWindow(s32 idx, f32 fovx, f32 fovy, f32 width, f32 height, f32 nearz, f32 farz) {
    View *v = &ZoidWinInfo[idx];
    f32 fx;
    f32 fy;
    f32 h;

    v->width = width;
    v->height = height;
    v->nearz = nearz;
    v->farz = farz;
    if (screen_height > 240) {
        v->aspect = width / (height / 2.0f);
    } else {
        v->aspect = width / height;
    }
    if (fovx > 0.0f) {
        fx = fovx * 0.017453292f;
        if (fovy > 0.0f) {
            fy = fovy * 0.017453292f;
        } else {
            if (screen_height > 240) {
                h = height / 2.0f;
            } else {
                h = height;
            }
            fy = fatan(tan_cody_waite(fx / 2.0f) * h / width) * 2;
        }
        v->ortho = 0;
    } else {
        fy = fx = 1.5707964f;
        v->ortho = 1;
    }
    v->fovx = fx;
    v->fovy = fy;
    v->tanx = tan_cody_waite(fx / 2.0f);
    v->tany = tan_cody_waite(v->fovy / 2.0f);
    v->invy = 0.5f / v->tany;
    v->invx = .5f / v->tanx;
    return v;
}
