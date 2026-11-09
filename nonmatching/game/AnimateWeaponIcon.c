#include "game/game.h"

typedef struct Visual {
    struct Visual *next;  /* 0x00 */
    s16 index;            /* 0x04 */
    s16 objnum;           /* 0x06 */
    s16 slot;             /* 0x08 */
    u32 data;             /* 0x0c */
    f32 timeStamp;        /* 0x10 */
    void (*func)(struct Visual *, s16); /* 0x14 */
} Visual;
typedef struct Target {
    u8 pad00[4];
    u8 Flags;             /* 0x04 */
    u8 pad05[0x0c - 5];
    s32 dlIdx;            /* 0x0c */
    u8 pad10[0x14 - 0x10];
    f32 uvs[3][3];        /* 0x14 (mat.mat3.uvs) */
    u8 pad38[0x50 - 0x38];
    s16 type;             /* 0x50 */
    u8 pad52[0x6c - 0x52];
    u32 data;             /* 0x6c: a window address (the node keeps its N64 layout) */
} Target;

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

void AnimateWeaponIcon(Visual *v, s16 op)
{
    Target *t;
    f32 *angV;
    s32 pad;
    f32 angVF[3];
    s32 i;
    s32 ok;
    s32 wep;
    if (!op) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState != 0) return;
    t = (Target *)((void *)PC_N64P(v->data));
    if (t->Flags & 2) {
        angV = ((void *)PC_N64P(t->data));
        angVF[0] = angV[0] * gFrameSeconds;
        angVF[1] = angV[1] * gFrameSeconds;
        angVF[2] = angV[2] * gFrameSeconds;
        RotateUV(angVF, t->uvs);
    } else if (ZOID_ObjectFlags(t->dlIdx) & 0x80000000) {
        switch (t->type) {
        case 350: wep = 0; break;
        case 351: wep = 1; break;
        case 352: wep = 2; break;
        case 355: wep = 3; break;
        case 356: wep = 4; break;
        case 357: wep = 5; break;
        case 358: wep = 6; break;
        case 360: wep = 7; break;
        default: wep = -1; break;
        }
        if (wep != -1) {
            ok = 1;
            for (i = 0; i < gNumPlayers; i++) {
                if (wep == PC_CAR_KIND(i)) ok = 0;
            }
            if (ok) {
                MBOX_ShowObject(t->dlIdx, 0, 15);
                t->Flags |= 2;
            }
        }
    } else {
        MBOX_HideObject(t->dlIdx, 0, 15);
    }
}
