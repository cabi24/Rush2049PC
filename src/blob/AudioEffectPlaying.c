#include "game/game.h"

typedef struct Entry {
    s8 active;      /* 0 */
    s8 b1;          /* 1 */
    u8 pad2[6];
    s32 id;         /* 8 */
    s32 pad12;
    u32 handle;     /* 16 */
    s32 pad20;
} Entry;            /* 24 */
VIEW(Entry *, gSfxInstances);
#define gSfxInstances gSfxInstances_view

s32 AudioEffectPlaying(s32 id) {
    u8 slot;
    Entry *e;
    if (id != -1) {
        slot = id;
        if (id == gSfxInstances[slot].id) {
            if (gSfxInstances[slot].b1 != 0) {
                e = gSfxInstances + slot;
                return sound_check(e->handle) != -1;
            }
            return 1;
        }
    }
    return 0;
}
