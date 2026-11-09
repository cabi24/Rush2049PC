/* StartListeners: a listener for each player's view at a race's start (MAT3 audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"

typedef struct Vec3 { f32 x, y, z; } Vec3;
/* The player record's fields this file reads (MAT3 tPlayer: slot, listener). */
typedef struct PlayerRec {
    /* 0x00 */ u8 slot;
    /* 0x01 */ u8 pad1[0x43];
    /* 0x44 */ s32 listener;   /* its number in gListeners, -1 for none */
    /* 0x48 */ void *ref;
} PlayerRec; /* 0x4C */
/* A player record keeps its listener as the listener's number in gListeners (AudInit3D's array), -1 for none: MAT3's
 * tPlayer keeps a pointer, which the record's 4 bytes at +0x44 can't hold (the save pointer after them fills the
 * 80-byte record). AudAddListener gives -1 (as a pointer) when the sound manager has no listeners. */
static s32 listener_number(tSfxListener *listener)
{
    return listener == (tSfxListener *)-1 ? -1 : (s32)(listener - gListeners);
}

extern PlayerRec gPlayers[];



/* Outside the demo: gives each player a listener at the origin, at rest, looking along z with y up (each frame
 * UpdateListeners moves it), except the second player of a ghost race (the ghost: -1), and fades the effects up to
 * the options' volume. */
void StartListeners(void)
{
    s32 i;

    if (!(gGameState & GS_DEMO)) {
        for (i = 0; i < gNumPlayers; i++) {
            if (gRace != kGhost || gPlayers[i].slot == 0)
                gPlayers[i].listener = listener_number(AudAddListener(&unitmatrix_origin, &unitmatrix_origin,
                                                                      (Vec3 *)unitmatrix[2], (Vec3 *)unitmatrix[1]));
            else gPlayers[i].listener = -1;
        }
        AudSetMasterVol((f32)gOptions[kOptEffectsVolume] / 10.0f, 0.1f, 0, 1);
    }
}
