#include "game/game.h"

/* PC: the ghost header keeps its N64 layout, file data; the profile record is 64 bytes (docs/port/POINTERS.md 9.10) */
#define N64W(x) PC_N64W(x)

typedef struct GhostHeader {      /* GC __anon_49776; N64 layout 0x58 */
    u32 headerChecksum;           /* 0x00 */
    u8 revision;                  /* 0x04 */
    s8 flag;                      /* 0x05 */
    s8 track;                     /* 0x06 */
    u8 backward;                  /* 0x07 */
    u8 car;                       /* 0x08 GC link car */
    u8 carTrans;                  /* 0x09 */
    s8 settings[12];              /* 0x0A handling, engine, tires, frame, wings, link 2/3/4, rims, horn, front, rear */
    u8 name[13];                  /* 0x16 */
    u8 pad23[0x28 - 0x23];
    u32 playerId[2];              /* 0x28 */
    u32 bestTime;                 /* 0x30 */
    f32 frameTime;                /* 0x34 GC 0x30 gRetraceTime */
    f32 finish;                   /* 0x38 */
    u32 zippedSize;               /* 0x3C */
    u32 unzippedSize;             /* 0x40 */
    u32 zippedChecksum;           /* 0x44 */
    u32 unzippedChecksum;         /* 0x48 */
    u32 start;                    /* 0x4C: a heap block (the window), 32 bits as in the file */
    u32 offset;                   /* 0x50 */
    s32 size;                     /* 0x54 */
} GhostHeader;

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
    GhostHeader **header;         /* 0x28 */
} Ghost;

typedef struct PlayerRec {        /* GC player record (0x4C); N64 0x30 */
    void *next; void *prev; void *fileh;
    u32 playerId[2];              /* 0x0C */
    u8 name[13];                  /* 0x14 */
    u8 pad21[44 - 33];
    u8 **datah;                   /* 0x2C */
} PlayerRec;

typedef struct Player {           /* GC gPlayers (0x50); N64 0x4C */
    u8 node;                      /* 0x00 */
    u8 slot;                      /* 0x01 */
    u8 pad2[66 - 2];
    u8 carTrans;                  /* 0x42 */
    u8 pad43[72 - 67];
    PlayerRec **handle;           /* 0x48 */
} Player;

typedef struct CarCfg { u8 s[16]; } CarCfg;
typedef struct TrackRec { u16 time; u8 pad[94]; } TrackRec;

void *memset(void *, s32, u32);
void *memcpy(void *, const void *, u32);

VIEW(PlayerRec[], gRealGhostPlayers);
#define gRealGhostPlayers gRealGhostPlayers_view

VIEW(GhostHeader[], gRealNewGhostHeaders);
#define gRealNewGhostHeaders gRealNewGhostHeaders_view

VIEW(Ghost[], gRealNewGhosts);
#define gRealNewGhosts gRealNewGhosts_view
VIEW(Ghost **[], gGhosts);
#define gGhosts gGhosts_view
extern Player gPlayers[];                                 /* GC gPlayers */

VIEW(s8[][13], gTireRims);
#define gTireRims gTireRims_view

void GhostStart(void) {
    GhostHeader *g;
    s32 i;
    s32 j;
    s32 np;
    s32 car;
    s32 n;
    Ghost *ghost;
    s32 bk;
    s32 size;

    n = expansion_pak_present ? 4 : 2;
    if (gRace != 2) {
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
        g->bestTime = ((TrackRec *)(*(*gPlayers[i].handle)->datah + 224) + bk)[gTrackNum].time;
        g->track = gTrackNum;
        g->backward = gBackwards;
        memcpy(g->name, (*gPlayers[i].handle)->name, 13);
        memcpy(g->playerId, (*gPlayers[i].handle)->playerId, 8);
        g->carTrans = GetPlayerCarTrans(gPlayers[i].handle);
        memcpy(g->settings, ((CarCfg *)(*(*gPlayers[i].handle)->datah + 1860))[PC_LINK_CAR(i)].s, 12);
        g->car = PC_LINK_CAR(i);
        g->frameTime = gRetraceTime;
        if (!gRestartGame) {
            g->size = (f32)(ghostRaceTimes[gTrackNum] + 10.0) * gRetraceRate;
            g->start = N64W(AllocateMemoryHeap(NULL, g->size * 3));
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
        (*ghost->header)->start = N64W(*ghost->datah);
        g = *(*gGhosts[i])->header;
        g->offset = 0;
        car = g->car;
        memset(&gPlayers[i], 0, 76);
        gPlayers[i].handle = &gGhostPlayers[i];
        memset(gGhostPlayers[i], 0, PROFILE_OFS(48));
        memcpy((*gPlayers[i].handle)->name, g->name, 13);
        gPlayers[i].node = i;
        gPlayers[i].slot = i;
        gPlayers[i].carTrans = g->carTrans;
        gCarHandling[i + 1][car] = g->settings[0];
        gCarEngines[i + 1][car] = g->settings[1];
        gCarTires[i + 1][car] = g->settings[2];
        gCarFrame[i + 1][car] = g->settings[3];
        gCarWings[i + 1][car] = g->settings[4];
        gTireRims[i + 1][car] = g->settings[8];
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
        PC_MODEL_DRONE_TYPE(np) = 2;
        PC_MODEL_DT(np) = g->frameTime;
        np++;
        gNumCars = np;
    }
    num_humans = gNumPlayers = np;
}
