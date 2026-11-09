#include "game/game.h"

VIEW(intptr_t, gBBoxModelStructs);
#define gBBoxModelStructs gBBoxModelStructs_view

void UpdateMiniModelStruct(s32 arg0, f32 *arg1, f32 *arg2, void *arg3) {
    f32 *p;
    f32 *q;

    p = (f32 *) ((arg0 << 6) + gBBoxModelStructs);
    q = arg1;
    CopyUV(arg3, (u8 *) p + 4);
    p[0x28 / 4] = arg2[0];
    p[0x2C / 4] = arg2[1];
    p[0x30 / 4] = arg2[2];
    p[0x34 / 4] = q[0];
    p[0x38 / 4] = q[1];
    p[0x3C / 4] = q[2];
}
