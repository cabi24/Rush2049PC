#include "game/game.h"

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

VIEW(M2C_UNK, targetInfo);
#define targetInfo targetInfo_view

VIEW(void *, gEnvList);
#define gEnvList gEnvList_view

/* callees outside the group */
intptr_t GrabEnvEntry();

/* PC: the target arg0 (a level-object node) keeps its N64 layout, but its visual is a struct pc_visual (data a
 * 32-bit window address) and its target info record is 0x40 bytes with the visual function a pointer; the m2c body
 * uses the N64 offsets of both (docs/port/CARS.md, POINTERS.md 10). */
void StartFence(void *arg0) {
    struct pc_visual *v;
    u8 *info;

    info = (u8 *) &targetInfo + M2C_FIELD(arg0, s16 *, 0x10) * TINFO_OFS(0x30);
    M2C_FIELD(arg0, s16 *, 0x5A) = 0xC;
    v = (struct pc_visual *) GrabEnvEntry();
    if (v != NULL) {
        v->index = 0;
        v->func = M2C_FIELD(info, void (**)(void *, short), TINFO_OFS(0xC));
        v->data = PC_N64W(arg0);
        v->timeStamp = (f32) fence_frame_time;
        M2C_FIELD(arg0, u8 *, 4) = (u8) (M2C_FIELD(arg0, u8 *, 4) & 0xFFF9);
        v->next = gEnvList;
        gEnvList = v;
        /* the target type's start sound, targetInfo[type].startSnd (N64 +0x1C) */
        PlayTargetSound(*(s32 *)((u8 *) &targetInfo + (M2C_FIELD(arg0, s16 *, 0x10) * TINFO_OFS(0x30)) + TINFO_OFS(0x1C)), M2C_FIELD(arg0, s8 *, 0x5C), (u8 *) arg0 + 0x38, 2);
    }
}

