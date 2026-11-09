#include "game/game.h"

VIEW(s32, gFreeEmitters);
#define gFreeEmitters gFreeEmitters_view

/* PC: an emitter (AudEmitEffect's Cam) has host-sized links, so its active flag is at +0x10
 * (docs/port/SYNTH.md). The list header keeps its head at +8. */
#define EMITTER_ACTIVE 0x10
s32 *NewEmitter(void) {
    s32 *sp1C;

    if (M2C_FIELD(&gFreeEmitters, s32 **, 8) == NULL) {
        return NULL;
    }
    sp1C = M2C_FIELD(&gFreeEmitters, s32 **, 8);
    RemoveLLE(&gFreeEmitters, M2C_FIELD(&gFreeEmitters, s32 **, 8));
    M2C_FIELD(sp1C, s8 *, EMITTER_ACTIVE) = 0;
    return sp1C;
}
