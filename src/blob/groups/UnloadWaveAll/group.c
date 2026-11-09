/* UnloadWaveAll (MAT3 waves.c): frees every wave instance that holds a given wave (waves/group.c has the waves). */
#include "game/game.h"

/* Frees every wave instance holding wave (both handle kinds). */
void UnloadWaveAll(s32 wave) {
    s32 i;
    for (i = 0; i < 64; i++) {
        unload_wave_if_matching(&gWaveInstance[i], i, wave, -1);
    }
}

/* Frees wave instance i (slot) if it holds wave and, unless handle is negative, has that handle kind. */
void unload_wave_if_matching(anon_loaded_ready_14 *slot, s32 i, s32 wave, s8 handle) {
    if ((slot->ptr != 0) && (wave == slot->wave) && ((handle < 0) || (handle == slot->handle))) UnloadWaveInstance(i);
}
