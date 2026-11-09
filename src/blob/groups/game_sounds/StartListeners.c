/* StartListeners: a listener for each player's view at a race's start (MAT3 audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"

typedef struct Vec3 { f32 x, y, z; } Vec3;
/* The player record's fields this file reads (MAT3 tPlayer: slot, listener). */
typedef struct PlayerRec {
    /* 0x00 */ u8 slot;
    /* 0x01 */ u8 pad1[0x43];
    /* 0x44 */ s32 listener;
    /* 0x48 */ void *ref;
} PlayerRec; /* 0x4C */
/* PC: an input record is 80 bytes; +0x44 stays 32 bits: a listener is a heap block, so its window address
 * (docs/port/POINTERS.md 10; pc_prelude.h's PC_N64A, docs/port/NOFIXED.md 2.3). */
#define LISTENER_WORD(p) (s32)PC_N64A(p)

extern PlayerRec gPlayers[];
VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(Vec3, unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view
VIEW(f32[][3], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(s8[], gOptions);
#define gOptions gOptions_view

/* gRace's ghost race, gOptions' effects volume (0 to 10); gGameState's demo bit is system.h's GAME_STATE_DEMO. */
#define RACE_GHOST 2
#define OPTION_FX_VOLUME 13

tSfxListener *AudAddListener(Vec3 *pos, Vec3 *dir, Vec3 *look, Vec3 *up);

/* Outside the demo: gives each player a listener at the origin, at rest, looking along z with y up (each frame
 * UpdateListeners moves it), except the second player of a ghost race (the ghost: -1), and fades the effects up to
 * the options' volume. */
void StartListeners(void)
{
    s32 i;

    if (!(gGameState & GAME_STATE_DEMO)) {
        for (i = 0; i < gNumPlayers; i++) {
            if (gRace != RACE_GHOST || gPlayers[i].slot == 0)
                gPlayers[i].listener = LISTENER_WORD(AudAddListener(&unitmatrix_origin, &unitmatrix_origin, (Vec3 *) unitmatrix[2], (Vec3 *) unitmatrix[1]));
            else gPlayers[i].listener = -1;
        }
        AudSetMasterVol((f32)gOptions[OPTION_FX_VOLUME] / 10.0f, 0.1f, 0, 1);
    }
}
