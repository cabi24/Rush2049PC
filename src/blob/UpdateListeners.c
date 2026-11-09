/* UpdateListeners: moves each player's listener with its car and camera, every frame of a race (MAT3 audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* A player record (80 bytes: its pointer at +0x48 is 8 bytes). Its listener at +0x44 is the listener's number in
 * gListeners (StartListeners), NO_LISTENER for none: MAT3's tPlayer keeps a pointer, which the record's 4 bytes there
 * can't hold. */
typedef struct CarRec {
    u8 model;           /* 0x0: the player's car slot (MAT3 slot) */
    u8 pad1[67];
    s32 listener;       /* 0x44: the listener (MAT3 listener): its number in gListeners, NO_LISTENER for none */
    void *ref;          /* 0x48 */
} CarRec;
#define NO_LISTENER (-1)


extern CarRec gPlayers[];

/* For each player with a listener: places it at the car's position (its model's rwr), at rest, looking along the
 * camera's forward axis with the camera's up. */
void UpdateListeners(void)
{
    s32 i;
    tSfxListener *listener;

    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].listener != NO_LISTENER) {
            listener = &gListeners[gPlayers[i].listener];
            AudUpdateListener(listener, PC_MODEL_RWR(gPlayers[i].model), unitmatrix_origin,
                            gCamera[i].mat.uvs[2], gCamera[i].mat.uvs[1]);
        }
    }
}
