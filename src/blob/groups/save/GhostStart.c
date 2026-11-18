/* GhostStart: a ghost race's ghosts, recorded and raced against (MAT3 ghost.c). */
#include "game/game.h"

typedef struct Player {           /* GC gPlayers (0x50); N64 0x4C */
    u8 node;                      /* 0x00 */
    u8 slot;                      /* 0x01 */
    u8 pad2[66 - 2];
    u8 carTrans;                  /* 0x42 */
    u8 pad43[72 - 67];
    tProfile **handle;            /* 0x48 */
} Player;

void *memset(void *, s32, u32);
void *memcpy(void *, const void *, u32);

extern Player gPlayers[];                                 /* GC gPlayers */

/* Sets a ghost race's ghosts up (only in a ghost race): each player gets a new ghost to record, its header from the
 * player (its number: the profile's ghost races on the track; name, id, car, transmission and settings, the frame
 * time) and, but on a restart, room for the race's time and 10 seconds of steps (3 bytes each). Then each saved
 * ghost (2 of them, 4 with the Expansion Pak) is read (ReadGhostData; one that can't be read drops out) and becomes
 * a player driving its car with its settings, colours and frame time, in a car slot after the humans. */
void GhostStart(void) {
    tGhostHeader *g;
    s32 i;
    s32 j;
    s32 np;
    s32 car;
    s32 n;
    tGhostFile *ghost;
    s32 bk;
    s32 size;

    n = expansion_pak_present ? 4 : 2;
    if (gRace != kGhost) {
        return;
    }
    if (!ghost_gInited) {
        for (i = 0; i < n; i++) {
            gGhostPlayers[i] = &gRealGhostPlayers[i];
            gNewGhostHeaders[i] = &gRealNewGhostHeaders[i];
        }
        ghost_gInited = 1;
    }
    if (gRestartGame) {
        gNumCars = 1;
        gNumPlayers = 1;
    }
    np = gNumPlayers;
    for (i = 0; i < gNumPlayers; i++) {
        gNewGhosts[i] = &gRealNewGhosts[i];
        gGhosts[i] = &gNewGhosts[i];
        g = gNewGhostHeaders[i];
        gRealNewGhosts[i].header = &gNewGhostHeaders[i];
        g->flag = -1;
        bk = gBackwards ? 6 : 0;
        g->id = (*(*gPlayers[i].handle)->datah)->raceStats[gTrackNum + bk].totalGhosts;
        g->track = gTrackNum;
        g->backward = gBackwards;
        memcpy(g->name, (*gPlayers[i].handle)->name, 13);
        memcpy(g->playerId, (*gPlayers[i].handle)->id, 8);
        g->carTrans = GetPlayerCarTrans(gPlayers[i].handle);
        memcpy(g->settings, (*(*gPlayers[i].handle)->datah)->carSettings[PC_LINK_CAR(i)].settings, 12);
        g->car = PC_LINK_CAR(i);
        g->frameTime = gRetraceTime;
        if (!gRestartGame) {
            /* the steps the recording has room for, converted as an s32 (the N64's) */
            g->size = (s32)((f32)(ghostRaceTimes[gTrackNum] + 10.0) * gRetraceRate);
            *ghost_recording(g) = AllocateMemoryHeap(NULL, g->size * 3);
            g->hasSamples = 1;
        }
        g->offset = 0;
    }
    for (; i < n; i++) {
        if (gGhosts[i] == NULL) {
            break;
        }
        if (ReadGhostData(gGhosts[i]) == 0) {
            for (j = i; j < 3; j++) {
                gGhosts[j] = gGhosts[j + 1];
            }
            continue;
        }
        ghost = *gGhosts[i];
        (*ghost->header)->hasSamples = 1;   /* the samples: its data, *ghost->datah (GhostUpdate) */
        g = *(*gGhosts[i])->header;
        g->offset = 0;
        car = g->car;
        memset(&gPlayers[i], 0, 76);
        gPlayers[i].handle = &gGhostPlayers[i];
        memset(gGhostPlayers[i], 0, sizeof(tProfile));
        memcpy((*gPlayers[i].handle)->name, g->name, 13);
        gPlayers[i].node = i;
        gPlayers[i].slot = i;
        gPlayers[i].carTrans = g->carTrans;
        gCarHandling[i + 1][car] = g->settings[0];
        gCarEngines[i + 1][car] = g->settings[1];
        gCarTires[i + 1][car] = g->settings[2];
        gCarFrame[i + 1][car] = g->settings[3];
        gCarWings[i + 1][car] = g->settings[4];
        TIRE_RIMS(i + 1)[car] = g->settings[8];
        gCarHorn[i + 1][car] = g->settings[9];
        gFrontTireScale[i + 1][car] = g->settings[10] / 100.0f + 0.75f;
        gRearTireScale[i + 1][car] = g->settings[11] / 100.0f + 0.75f;
        PC_LINK_TRACK(np) = g->track;
        PC_LINK_CAR(np) = car;
        PC_LINK_COLOR0(np) = g->settings[5];
        PC_LINK_COLOR1(np) = g->settings[6];
        PC_LINK_COLOR2(np) = g->settings[7];
        PC_LINK_PLACE(np) = np;
        PC_LINK_FLAGS(np) = 176;
        PC_LINK_OWNER(np) = 6;
        pc_models[np].drone_type = 2;
        pc_models[np].fastin.dt = g->frameTime;
        np++;
        gNumCars = np;
    }
    num_humans = gNumPlayers = np;
}
