/* UpdateListeners: moves each player's listener with its car and camera, every frame of a race (MAT3 audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* A player's listener (tPadState.listener) is its number in gListeners (StartListeners), NO_LISTENER for none. */
#define NO_LISTENER (-1)

/* For each player with a listener: places it at the car's position (its model's rwr), at rest, looking along the
 * camera's forward axis with the camera's up. */
void UpdateListeners(void)
{
    s32 i;
    tSfxListener *listener;

    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].listener != NO_LISTENER) {
            listener = &gListeners[gPlayers[i].listener];
            AudUpdateListener(listener, PC_MODEL_RWR(gPlayers[i].slot), unitmatrix_origin,
                            gCamera[i].mat.uvs[2], gCamera[i].mat.uvs[1]);
        }
    }
}
