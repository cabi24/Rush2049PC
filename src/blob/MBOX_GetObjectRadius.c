#include "game/game.h"

VIEW(s32, gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view

typedef struct { u8 *ptr; s32 count; } ABBEnt;
/* PC: gRomObjDefPtrs entries are {model *, s32}, 16 bytes; the model records keep their N64 layout. */
#define ABB_STRIDE sizeof(ABBEnt)
f32 MBOX_GetObjectRadius(s32 arg0) {
    s32 idx;
    s32 sub;
    ABBEnt *e;

    idx = arg0 >> 0xA;
    if (idx >= (s32) *(volatile u8 *) gNumModels) {
        return 0.0f;
    }
    e = (ABBEnt *) ((u8 *) &gRomObjDefPtrs + idx * ABB_STRIDE);
    sub = arg0 & 0x3FF;
    if (sub >= e->count) {
        return 0.0f;
    }
    return *(f32 *) (e->ptr + sub * 0x58 + 0x10);
}
