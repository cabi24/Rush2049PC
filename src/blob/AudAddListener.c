/* AudAddListener: adds a listener for the 3D sounds (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

typedef struct Vec3 {float x,y,z;} Vec3;

/* MAT3's _NewListener (the N64 keeps its own out-of-line copy, NewListener, which nothing calls): takes the first
 * free listener and marks it taken. */
static tSfxListener *take_free_listener(void) {
    tSfxListener *listener;
    listener = (tSfxListener *)gFreeListeners.head;
    RemoveLLE(&gFreeListeners, &listener->ll);
    listener->free = 0;
    return listener;
}

/* MAT3's _ActivateListener: puts the listener on the active list, where the emitters hear it. */
static void activate_listener(tSfxListener *listener) {
    InsertLLE(&gActiveListeners, &listener->ll, gActiveListeners.head);
    listener->active = 1;
}

/* Adds a listener at pos moving with dir, looking along look with up, and returns it (the caller keeps it, as a
 * window address, in its player record: StartListeners); -1 when the sound manager has no listeners
 * (AudInit3D). */
tSfxListener *AudAddListener(Vec3 *pos,Vec3 *dir,Vec3 *look,Vec3 *up) {
    tSfxListener *listener;
    if(!gListeners) return (tSfxListener *)-1;
    audio_lock();
    if (gFreeListeners.head) {}
    listener = take_free_listener();
    listener->pos[0]=pos->x;
    listener->pos[1]=pos->y;
    listener->pos[2]=pos->z;
    listener->dir[0]=dir->x;
    listener->dir[1]=dir->y;
    listener->dir[2]=dir->z;
    listener->look[0]=look->x;
    listener->look[1]=look->y;
    listener->look[2]=look->z;
    listener->up[0]=up->x;
    listener->up[1]=up->y;
    listener->up[2]=up->z;
    activate_listener(listener);
    audio_unlock();
    return listener;
}
