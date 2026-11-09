/* UpdateEnvirons.c: animate the environment visuals for a frame (MAT3 visuals.c, UpdateEnvirons()). */
#include "game/game.h"

/* A visual (tVisual) on the PC: 0x20 bytes, next and func host pointers (docs/port/CARS.md 3). */
typedef struct tVisual {
    struct tVisual *next;
    u8 pad8[0x18 - 0x8];        /* index, objnum, slot, data, timeStamp */
    M2C_UNK (*func)(struct tVisual *v, M2C_UNK op);   /* its animator */
} tVisual;

/* Call each visual's animator on gEnvList with op 1 (update), then the skid marks'. The next visual is read before
 * the call, since an animator may release its own visual (ReleaseEnviron unlinks it). */
void UpdateEnvirons(void) {
    M2C_UNK (*func)(tVisual *, M2C_UNK);
    tVisual *v;
    tVisual *next;

    v = gEnvList;
    if (gEnvList != 0) {
        next = gEnvList->next;
    } else {
        next = 0;
    }
    if (v != 0) {
        do {
            func = v->func;
            if (func != NULL) {
                func(v, 1);
            }
            v = next;
            if (next != 0) {
                next = v->next;
            }
        } while (v != 0);
    }
    AnimateSkids();
}
