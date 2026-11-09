/* UpdateListeners: moves each player's listener with its car and camera, every frame of a race (MAT3 audio.c). */
#include "game/game.h"

/* PC: a player record is 80 bytes (its pointer at +0x48 is 8 bytes). The listener at +0x44 stays 32 bits: it is a
 * window address (a heap block, AudAddListener) or -1 (docs/port/POINTERS.md 10). */
typedef struct CarRec {
    u8 model;           /* 0x0: the player's car slot (MAT3 slot) */
    u8 pad1[67];
    u32 listener;       /* 0x44: the listener (MAT3 listener), NO_LISTENER for none */
    void *ref;          /* 0x48 */
} CarRec;
#define NO_LISTENER 0xFFFFFFFFu
#define LISTENER(n) (void *)PC_N64P(n)

/* The camera record's fields this file reads: its matrix's forward and up rows (MAT3 mat.mat4.uvs[2], [1]). */
typedef struct View { u8 pad0[12]; f32 up[3]; f32 forward[3]; u8 pad24[152 - 36]; } View;  /* 152 */

extern CarRec gPlayers[];
VIEW(f32[], unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view
VIEW(View[], gCamera);
#define gCamera gCamera_view

/* For each player with a listener: places it at the car's position (its model's rwr), at rest, looking along the
 * camera's forward axis with the camera's up. */
void UpdateListeners(void)
{
    s32 i;
    void *listener;

    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].listener != NO_LISTENER) {
            listener = LISTENER(gPlayers[i].listener);
            AudUpdateListener(listener, PC_MODEL_RWR(gPlayers[i].model), unitmatrix_origin,
                            gCamera[i].forward, gCamera[i].up);
        }
    }
}
