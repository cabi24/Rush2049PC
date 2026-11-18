/* StartListeners: a listener for each player's view at a race's start (MAT3 audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"

typedef struct Vec3 { f32 x, y, z; } Vec3;
/* A player's input record keeps its listener (tPadState.listener) as the listener's number in gListeners
 * (AudInit3D's array), -1 for none: MAT3's tPadState keeps a pointer, which the record's 4 bytes there can't hold.
 * AudAddListener gives -1 (as a pointer) when the sound manager has no listeners. */
static s32 listener_number(tSfxListener *listener)
{
    return listener == (tSfxListener *)-1 ? -1 : (s32)(listener - gListeners);
}

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
