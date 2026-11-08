/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* props_render (0x800F6AB8, 545 words) = GC ghost.c GhostStart, N64 form (label is historical:
 * the function sets up the ghost players at race start). Fresh draft from the GC disassembly
 * (reference/gc/disasm/ghost.c/GhostStart.s) + the N64 words. EQUAL in the whole-program unit
 * (the whole-program unit --tag score props_render --with THIS --internal ReadGhostData --neighbours: 0 locked differ).
 * GC-derived names (source GhostStart unless noted): gRace, gInited = ghost_gInited,
 *   gRestartGame, gNumCars = gNumCars (volatile, as in animRadar/fake_init),
 *   gNumPlayers, num_humans = num_humans (volatile), gGhostPlayers
 *   (player records gRealGhostPlayers, 0x30 each) / gNewGhostHeaders (header pointers to gRealNewGhostHeaders, 0x58 each) /
 *   gNewGhosts (ghost slots to gRealNewGhosts ghosts, 0x2C each), gGhosts, gPlayers
 *   (0x4C each: node 0, slot 1, carTrans 0x42, handle 0x48), gLink = D_80153E88 (8 B each), model = D_8014A250
 *   (0x808 each: frameTime 0x718, state 0x7CC), gTrackNum, gBackwards, gRetraceTime =
 *   gRetraceTime, gRetraceRate, ghostRaceTimes, gCarHandling/Engines/Tires/Frame/Wings/
 *   TireRims/Horn = gCarHandling/gCarEngines/gCarTires/gCarFrame/gCarWings/gTireRims/gCarHorn ([5][13] s8),
 *   gFrontTireScale/gRearTireScale = gFrontTireScale/gRearTireScale ([5][13] f32), GetPlayerCarTrans,
 *   AllocateMemoryHeap = audio_dma_sync, _ReadGhostData = ReadGhostDataInternal (IPA ghosth s1, decode s0, select s3).
 * N64 differences from GC: the slot count n = expansion_pak_present ? 4 : 2 (GC 4); the ghost header's best time comes
 *   from the player data's TrackRec table at +0xE0 (96 B per entry, +6 entries when backwards); gLink has three
 *   more header bytes (settings[5..7]); the per-player car settings are 12 bytes from data + 0x744 + car*16.
 * Helper: ReadGhostData = GC save.c ReadGhostData(ghosth) { return _ReadGhostData(ghosth, 1, 1); } (GC passes
 *   the same 1, 1), non-static, inlined, not on keep (its parameter is retail's v0 copy before `move s1,v0`).
 * Source-form evidence (no devices, no unused locals): `bk`/`size` are named locals (retail frame 184 and the
 *   named-slot offsets n 160 / np 168 need g, i, j, np, car, n, ghost, bk, size in this order); the car-settings
 *   source is indexed as CarCfg[car] so ugen evaluates the data chain before car*16; the final chained store
 *   `num_humans = gNumPlayers = np` sets the temp-ring registers of the tail. */
typedef signed char s8;typedef unsigned char u8;typedef short s16;typedef unsigned short u16;
typedef signed int s32;typedef unsigned int u32;typedef float f32;
#define NULL ((void *)0)
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

typedef struct Link {             /* GC gLink (0x0A); N64 0x08 */
    s8 track;
    u8 car;
    s8 b2, b3, b4;
    u8 np;
    u8 b6, b7;
} Link;

typedef struct Model { u8 pad[0x718]; f32 frameTime; u8 pad71C[0x7CC - 0x71C]; u8 state; u8 pad7CD[0x808 - 0x7CD]; } Model;
typedef struct CarCfg { u8 s[16]; } CarCfg;
typedef struct TrackRec { u16 time; u8 pad[94]; } TrackRec;

void *memset(void *, s32, u32);
void *memcpy(void *, const void *, u32);
u8 GetPlayerCarTrans(PlayerRec **handle);                       /* GC GetPlayerCarTrans */
void *AllocateMemoryHeap(void *heap, u32 size);                 /* GC AllocateMemoryHeap */
s32 ReadGhostDataInternal(Ghost **ghosth, s32 decode, s32 select);  /* GC _ReadGhostData (IPA s1, s0, s3) */
/* The game's own ReadGhostData (this file had a second copy of it: the same code). */
s32 ReadGhostData(Ghost **ghosth);

extern s8 expansion_pak_present;
extern s32 gRace;                                   /* GC gRace */
extern s8 ghost_gInited;                                       /* GC gInited */
extern s8 gRestartGame;                                       /* GC gRestartGame */
extern volatile s16 gNumCars;                                      /* GC gNumCars */
extern s16 gNumPlayers;                             /* GC gNumPlayers */
extern volatile s16 num_humans;                                      /* GC num_humans */
extern PlayerRec *gGhostPlayers[];                             /* GC gGhostPlayers[0..3] */
extern PlayerRec gRealGhostPlayers[];
extern GhostHeader *gNewGhostHeaders[];                           /* GC gGhostPlayers + 0x140 */
extern GhostHeader gRealNewGhostHeaders[];
extern Ghost *gNewGhosts[];                                 /* GC gGhostPlayers + 0x360 */
extern Ghost gRealNewGhosts[];                                  /* GC gGhostPlayers + 0x2A0 */
extern Ghost **gGhosts[];                                /* GC gGhosts */
extern Player gPlayers[];                                 /* GC gPlayers */
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gTrackNum;                                       /* GC gTrackNum */
extern s8 gBackwards;                                       /* GC gBackwards */
extern f32 gRetraceTime;                                      /* GC gRetraceTime */
extern f32 gRetraceRate;                                      /* GC gRetraceRate */
extern f32 ghostRaceTimes[];                                    /* GC ghostRaceTimes */
extern s8 gCarHandling[][13];                                 /* GC gCarHandling */
extern s8 gCarEngines[][13];                                 /* GC gCarEngines */
extern s8 gCarTires[][13];                                 /* GC gCarTires */
extern s8 gCarFrame[][13];                                 /* GC gCarFrame */
extern s8 gCarWings[][13];                                 /* GC gCarWings */
extern s8 gTireRims[][13];                                 /* GC gTireRims */
extern s8 gCarHorn[][13];                                 /* GC gCarHorn */
extern f32 gFrontTireScale[][13];                                /* GC gFrontTireScale */
extern f32 gRearTireScale[][13];                                /* GC gRearTireScale */

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
