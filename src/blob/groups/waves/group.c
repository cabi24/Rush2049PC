/* The background loader thread and a wave instance's model flag (MAT3 waves.c). A "wave" is one of the game's
 * asset files (a model WAD, a font, a sound bank ...), loaded into a heap block and kept in one of the 64 wave
 * instances (gWaveInstance, anon_loaded_ready_14). loadwave.c finds and loads waves, waves.c reads and sets them up,
 * replacewave.c swaps one for another, slot_sound/ locks and looks them up, heap_release/UnloadWaveInstance.c and
 * UnloadWaveAll/ free them, InitWaves.c frees all but the locked ones. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

extern void osSetThreadPri(OSThread *thread, OSPri priority);

/* The background loader thread (MAT3 BGLoadWaveMain; main_thread_entry starts it). Waits for a request from
 * _BGLoadWave (the wave instance to read, in bg_load_request), reads the wave (ReadWave) and says it is done on
 * bg_load_idle_queue, which _BGLoadWave and InitWaves wait on before the next request. */
void BGLoadWaveMain(void *arg) {
    OSMesg msg;

    osCreateMesgQueue((OSMesgQueue *) &bg_load_request_queue, (void **) &bg_load_request_messages, 1);
    osSetThreadPri(&bg_load_thread, THREAD_PRI_BG_LOAD);
    for (;;) {
        osRecvMesg((OSMesgQueue *) &bg_load_request_queue, &msg, 1);
        ReadWave(bg_load_request);
        osSendMesg((OSMesgQueue *) &bg_load_idle_queue, NULL, 1);
    }
}

/* Marks wave instance index as a model or not, by its wave (wave_is_model), and raises gNumModels, the count of
 * instances the model searches look through, to cover it (MAT3 _SetWaveInstanceModel). */
void SetWaveInstanceModel(s32 index) {
    if ((gWaveInstance[index].model = wave_is_model[gWaveInstance[index].wave]) != 0) {
        if (!(index < gNumModels[0])) {
            gNumModels[0] = index + 1;
        }
    }
}
