/* GhostEnd: a ghost race's end (MAT3 ghost.c). */
#include "game/game.h"

typedef struct Ghost {            /* GC __anon_49a9c; N64 layout 0x2C */
    void *ll;                     /* 0x00 */
    void **fileh;                 /* 0x04 */
    s8 track;                     /* 0x08 */
    u8 backward;                  /* 0x09 */
    u8 name[13];                  /* 0x0A */
    u8 pad17;
    u32 playerId[2];              /* 0x18 */
    f32 finish;                   /* 0x20 */
    u8 **datah;                   /* 0x24 */
    tGhostHeader **header;        /* 0x28 */
} Ghost;

/* Ends a ghost race (MAT3 GhostEnd): each player's ghost stops; a ghost played back drops its data
 * (PurgeGhostData); a recording of a car that finished keeps its time and length and becomes the ghost to save
 * (gGhostToSave, which the high score screen compresses); one that didn't finish is dropped. */
void GhostEnd(void)
{
    tGhostHeader *g;
    s32 i;

    gGhostToSave = 0;
    if (gRace != kGhost) {
        return;
    }
    gEndingGhosts = 1;
    for (i = 0; i < gNumPlayers; i++) {
        if (gGhosts[i] != 0) {
            Ghost **ghost = gGhosts[i];
            gGhosts[i] = 0;
            PC_MODEL_MODELRUN(i) = 0;
            g = *(*ghost)->header;
            if (g->flag >= 0) {
                if (g->flag > 0) {
                    PurgeGhostData(ghost);
                }
            } else {
                g->flag = 0;
                if (PC_CAR_PLACE_LOCKED(i) != 0) {
                    g->finish = PC_CAR_SCORE(i);
                    g->size = g->offset;
                    gGhostToSave = g;
                } else {
                    /* PC: the game frees the unfinished recording's block here (MAT3: DeallocateMemory(g->start);
                     * the N64 inlined that free). The PC's call went to an empty stub, so the block has stayed
                     * allocated on every PC build: kept so, as the gate requires (docs/port/PCONLY.md 4). */
                    *ghost_recording(g) = NULL;
                    g->hasSamples = 0;
                }
            }
        }
    }
    gNumPlayers = 1;
    gEndingGhosts = 0;
}
