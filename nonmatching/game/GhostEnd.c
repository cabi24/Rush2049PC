/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * GhostEnd (MAT3 ghost.c GhostEnd): the end of a ghost race (gRace 2). Every player's ghost is let go
 * (gGhosts[i], the car model's modelrun); then, by its recording header's flag, a ghost played back (flag > 0)
 * has its data purged (PurgeGhostData), and the recording of a player's run (flag < 0) ends: a finished car's
 * keeps its time and length and becomes gGhostToSave, an unfinished one is dropped. One player again after it.
 * nonmatching/README.md: the C was written from the N64's 440 bytes, the GameCube build's GhostEnd its model.
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;

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

typedef struct GhostPlayer { u8 pad0[239]; s8 ready; f32 value; u8 tail[0x110 - 0xF4]; struct pc_visual visuals[21]; u8 tail_308[0x380 - 0x308]; void *input; u8 tail_384[0x3B8 - 0x384]; } GhostPlayer;
typedef struct GhostObject { u8 pad0[1820]; s16 reset; u8 tail[234]; } GhostObject;
extern Ghost **gGhosts[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern GhostHeader *gGhostToSave;
extern s16 gNumPlayers;
extern s32 gRace;
extern volatile s8 gEndingGhosts;
extern void PurgeGhostData(Ghost **handle);

void GhostEnd(void)
{
    GhostHeader *g;
    s32 i;

    gGhostToSave = 0;
    if (gRace != 2) {
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
                if (PC_CAR_FINISHED(i) != 0) {
                    g->finish = PC_CAR_TIME(i);
                    g->size = g->offset;
                    gGhostToSave = g;
                } else {
                    /* PC: the game frees the unfinished recording's block here (MAT3: DeallocateMemory(g->start);
                     * the N64 inlined that free). The PC's call went to an empty stub, so the block has stayed
                     * allocated on every PC build: kept so, as the gate requires (docs/port/PCONLY.md 4). */
                    g->start = 0;
                }
            }
        }
    }
    gNumPlayers = 1;
    gEndingGhosts = 0;
}
