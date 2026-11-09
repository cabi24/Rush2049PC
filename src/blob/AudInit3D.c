/* AudInit3D: the sound manager's start-up of its 3D sound: the listeners and emitters (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* Called once by AudioInit. Empties the four lists and, given both counts, allocates numListeners listeners and
 * numEmitters emitters from the heap and puts them all on the free lists. footUnits scales the speed of sound
 * (1100 feet a second) to the world's units; frontDistance and doppler are kept for the emitters (gFrontDistance,
 * gCalcDoppler). */
void AudInit3D(s32 numListeners, s32 numEmitters, f32 footUnits, f32 frontDistance, s32 doppler) {
    s32 i;

    sound_list_init(&gFreeListeners, 0, 1);
    sound_list_init(&gActiveListeners, 0, 1);
    sound_list_init(&gFreeEmitters, 0, 1);
    sound_list_init(&gActiveEmitters, 0, 1);
    if (numListeners > 0 && numEmitters > 0) {
        gNumListeners = numListeners;
        gListeners = AllocateMemoryHeap(0, gNumListeners * sizeof(tSfxListener));
        PurgeableMemory(gListeners, 0);
        for (i = 0; i < gNumListeners; i++) {
            gListeners[i].free = 1;
            gListeners[i].active = 0;
            InsertLLE(&gFreeListeners, &gListeners[i].ll, gFreeListeners.head);
        }
        gNumEmitters = numEmitters;
        gEmitters = AllocateMemoryHeap(0, gNumEmitters * sizeof(tSfxEmitter));
        PurgeableMemory(gEmitters, 0);
        for (i = 0; i < gNumEmitters; i++) {
            gEmitters[i].free = 1;
            gEmitters[i].active = 0;
            InsertLLE(&gFreeEmitters, &gEmitters[i].ll, gFreeEmitters.head);
        }
        gSoundSpeed = 1100.0f * footUnits;
        gFrontDistance = frontDistance;
        gCalcDoppler = doppler;
    }
}
