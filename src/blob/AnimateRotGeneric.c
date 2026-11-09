#include "game/game.h"

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* PC: arg0 is a visual (struct pc_visual) whose data is the target, a level-object node in its N64 layout; both the
 * data and the target's state (+0x6C) are 32-bit window addresses (docs/port/CARS.md, POINTERS.md 10). */
#define VIS_DATA(v) (void *) PC_N64P(M2C_FIELD(v, u32 *, VIS_OFS(0xC)))
#define NODE_STATE(t) (void *) PC_N64P(M2C_FIELD(t, u32 *, 0x6C))
void AnimateRotGeneric(void *arg0, s16 arg1) {
    void *temp_v0;
    void *temp_v1;
    s32 reserved;
    f32 xyz[3];

    if (arg1 == 0) {
        ReleaseEnviron(arg0, 1);
        return;
    }
    if (gPauseState == 0) {
        temp_v1 = VIS_DATA(arg0);
        temp_v0 = NODE_STATE(temp_v1);
        xyz[0] = M2C_FIELD(temp_v0, f32 *, 0) * gFrameSeconds;
        xyz[1] = M2C_FIELD(temp_v0, f32 *, 4) * gFrameSeconds;
        xyz[2] = M2C_FIELD(temp_v0, f32 *, 8) * gFrameSeconds;
        RotateUV(xyz, (u8 *) temp_v1 + 0x14);
    }
}

