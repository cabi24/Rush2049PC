#include "game/game.h"

/* PC: the audio manager's sorted-tag node (anon_te_tag_c: two tree links, then the fx tag) and fx tag
 * (tFxSequence) have host-sized pointers: the tag is at +0x10, its weighted priority at +0x28
 * (docs/port/SYNTH.md). */
#define STAG_TAG 0x10
#define FX_WPRIORITY 0x28
s32 OrderFx(void *arg0, void *arg1) {
    return M2C_FIELD(M2C_FIELD(arg0, void **, STAG_TAG), s32 *, FX_WPRIORITY) - M2C_FIELD(M2C_FIELD(arg1, void **, STAG_TAG), s32 *, FX_WPRIORITY);
}
