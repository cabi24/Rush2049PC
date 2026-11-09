/* NewEmitter: takes a free 3D sound source (MAT3 audiomgr.c's _NewEmitter). */
#include "game/game.h"
#include "game/audiomgr.h"

/* Takes the first emitter off gFreeEmitters and marks it taken (free = 0), or returns NULL when none is free.
 * Called by AudEmitEffect under the audio lock. */
tSfxEmitter *NewEmitter(void) {
    tSfxEmitter *emitter;

    if (gFreeEmitters.head == NULL) {
        return NULL;
    }
    emitter = (tSfxEmitter *) gFreeEmitters.head;
    RemoveLLE(&gFreeEmitters, gFreeEmitters.head);
    emitter->free = 0;
    return emitter;
}
