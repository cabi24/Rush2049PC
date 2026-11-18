/* The background loader thread and a wave instance's model flag (MAT3 waves.c). A "wave" is one of the game's
 * asset files (a model WAD, a font, a sound bank ...), loaded into a heap block and kept in one of the 64 wave
 * instances (gWaveInstance, anon_loaded_ready_14). loadwave.c finds and loads waves, waves.c reads and sets them up,
 * replacewave.c swaps one for another, slot_sound/ locks and looks them up, heap_release/UnloadWaveInstance.c and
 * UnloadWaveAll/ free them, InitWaves.c frees all but the locked ones. */
#include "game/game.h"

/* The background loader thread (MAT3 BGLoadWaveMain; main_thread_entry starts it). Waits for a request from
 * _BGLoadWave (the wave instance to read, in bg_load_request), reads the wave (ReadWave) and says it is done on
 * bg_load_idle_queue, which _BGLoadWave and InitWaves wait on before the next request. */
void BGLoadWaveMain(void *arg) {
    OSMesg msg;

    queue_init(&bg_load_request_queue, &bg_load_request_messages, 1);
    thread_set_priority(&bg_load_thread, THREAD_PRI_BG_LOAD);
    for (;;) {
        msg = queue_recv(&bg_load_request_queue);
        ReadWave(bg_load_request);
        queue_send(&bg_load_idle_queue, NULL);
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
