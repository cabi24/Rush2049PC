/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * LoadTrack (0x800BB9B0, 2,104 B) = MAT3/GC/DC tracks.c LoadTrack(track, background, reloading), N64 version:
 *   load + wait on the track / world / collision / path waves (LoadWave, WaveInstanceReady =
 *   WaveInstanceReady, GetWaveInstanceAddr; N64 wave numbers track+101/+120/+139, path 158/177),
 *   world wad setup (func_80096C28 = MBSetupWad, lookup_with_output = MBGetFromWad: WDHR / GDAT / GTLD), the
 * "other" .81 by race/track/window count, then once: initTargetStructs, SkyInit
 *   (SkyInit), NewWorld (NewWorld, locked, inlined), InitEnvs, InitTrackDynamicObjs, LoadCars,
 *   InitVisuals, InitCheckpointData (locked: clears D_80110680[0..1] and calls InitCheckpointData; inlined),
 *   InitPathData, InitDemoPathPoint, InitSkids, InitSmoke, InitBlast and the image-B call initWorldDamage (GC
 *   initWorldDamage) when expansion_pak_present or track >= 6.
 * Base: draft (39/526 words). levers (39 -> 0):
 *   - the Dreamcast tracks.c keeps three empty gGameState & 8 checks after the gRace tests (`if (x & 8) {} else
 *     if (x & 8) {}` + `if (x & 8) {}`); they compile to nothing but supply the missing t6-t9 ring pop
 *     (workbench L65 class) -> 12 words;
 *   - NewWorld is the locked NewWorld (its out-parameter store gives retail's separate lui at / lui a0
 *     WorldBase store and reload) and the D_80110680 clear is the locked InitCheckpointData, both called and
 *     umerge-inlined, instead of the draft's private NewWorld/FindWorld/clear copies -> 0;
 *   - `track >= 6` for the final condition (uopt turns it into retail's &gWorldNames[track] compare).
 * No shaping devices: the empty checks are the Dreamcast decomp's source, every helper is a real locked function.
 */
typedef signed char s8;typedef unsigned char u8;typedef short s16;typedef unsigned short u16;
typedef signed int s32;typedef unsigned int u32;typedef float f32;
#define NULL ((void *)0)

typedef struct LoadState {     /* GC tracks.c __anon_10faf9 */
    s8 loading;                /* 0x00 */
    s8 loaded;                 /* 0x01 */
    s8 inited;                 /* 0x02 */
    s8 ready;                  /* 0x03 */
    s32 idx;                   /* 0x04 */
} LoadState;
typedef struct TrackLoadInfo { /* GC tracks.c __anon_10fbbb (gTrackLoadInfo), 0x2C */
    s8 trackInited;            /* 0x00 */
    LoadState track;           /* 0x04 */
    LoadState world;           /* 0x0C */
    LoadState collision;       /* 0x14 */
    LoadState path;            /* 0x1C */
    LoadState other;           /* 0x24 */
} TrackLoadInfo;
typedef struct WorldHdr {      /* GC world.c __anon_10fd25 */
    u32 woff;                  /* 0x00 */
    char wname[16];            /* 0x04 */
} WorldHdr;
typedef struct Wad {           /* GC mb_wad.c __anon_87656 */
    u32 *base;
    s32 TOCentries;
    void *TOC;
} Wad;

extern TrackLoadInfo gTrackLoadInfo;
extern s32 *gWorldData;
extern WorldHdr *WorldTable;
extern void *gTargetGroups;
extern s32 numgTargetGroups;
extern void *gTargetGroupList;
extern s32 numgTargetGroupList;
extern void *gRoadDat;
extern void *gPathWave;
extern s8 gBackwards;
extern u32 gGameState;
extern s32 gRace;
extern s8 expansion_pak_present;
extern s8 gTrackNum;
extern s16 gNumWindows;
extern char *gWorldNames[];         /* gWorldNames (N64: indexed by track) */

/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
s32 LoadWave(s32 wave, s32 a1, s32 background, s32 a3, void *heap);
s8 WaveInstanceReady(s32 idx);                                            /* GC WaveInstanceReady */
void *GetWaveInstanceAddr(s32 idx);                                           /* GC GetWaveInstanceAddr */
void MBSetupWad(Wad *wad, void *ptr);                                 /* GC MBSetupWad */
u32 MBGetFromWad(Wad *wad, s32 tag, s32 *count);       /* a WAD entry: a 32-bit window address */
void initTargetStructs(s16 track);                                    /* GC initTargetStructs */
void SkyInit(void);                                                 /* GC SkyInit */
void InitEnvs(void);                                                /* GC InitEnvs */
void InitTrackDynamicObjs(void);                                            /* GC InitTrackDynamicObjs */
void LoadCars(s32 x);                                     /* GC LoadCars */
void InitVisuals(s32 x);                                             /* GC InitVisuals */
void InitPathData(void);                                          /* GC InitPathData */
void InitDemoPathPoint(void);                                                /* GC InitDemoPathPoint */
void InitSkids(void);                                                /* GC InitSkids */
void InitSmoke(void);                                       /* GC InitSmoke */
void InitBlast(void);                                           /* GC InitBlast */
void initWorldDamage(void);                                                /* overlay call (GC initWorldDamage?) */

s16 NewWorld(u8 *World, s16 Parent);                        /* GC NewWorld (world.c, locked) */
void InitCheckpointData(void);                                            /* N64 InitCheckpointData wrapper (locked) */

/* PC: the GTLD keys become object pointers at race setup (SetTargetDefs), which don't fit the file's 4-byte
 * slots, so LoadTrack gets GTLD as a native array of the keys (port/assets/world.c); other chunks are returned
 * as they are. */
uintptr_t pc_world_chunk(u32 addr, s32 tag, s32 *count);
#define MBGetFromWad(wad, tag, count) pc_world_chunk(MBGetFromWad(wad, tag, count), tag, count)

/* The world headers' offsets become 32-bit N64 addresses: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
s32 LoadTrack(s32 track, s32 background, s32 reloading) {
    Wad wad;
    WorldHdr *whdr;
    u8 *worldptr;
    s32 i;

    if (!gTrackLoadInfo.track.loading) {
        gTrackLoadInfo.track.idx = LoadWave(track + 101, 0, background, 0, 0);
        gTrackLoadInfo.track.loading = 1;
    }
    if (!WaveInstanceReady(gTrackLoadInfo.track.idx)) {
        return 0;
    }
    gTrackLoadInfo.track.loaded = 1;
    gTrackLoadInfo.track.inited = 1;
    gTrackLoadInfo.track.ready = 1;
    if (!gTrackLoadInfo.world.loading) {
        gTrackLoadInfo.world.idx = LoadWave(track + 120, 0, background, 0, 0);
        gTrackLoadInfo.world.loading = 1;
    }
    if (!WaveInstanceReady(gTrackLoadInfo.world.idx)) {
        return 0;
    }
    gTrackLoadInfo.world.loaded = 1;
    if (!gTrackLoadInfo.world.ready) {
        worldptr = GetWaveInstanceAddr(gTrackLoadInfo.world.idx);
        MBSetupWad(&wad, worldptr);
        gWorldData = MBGetFromWad(&wad, 0x57484452, NULL);
        WorldTable = (WorldHdr *)(gWorldData + 1);
        if (!gTrackLoadInfo.world.inited) {
            for (i = 0, whdr = WorldTable; i < *gWorldData; i++, whdr++) {
                whdr->woff += N64A(worldptr);
            }
            gTrackLoadInfo.world.inited = 1;
        }
        gTargetGroups = MBGetFromWad(&wad, 0x47444154, &numgTargetGroups);
        gTargetGroupList = MBGetFromWad(&wad, 0x47544C44, &numgTargetGroupList);
        gTrackLoadInfo.world.ready = 1;
    }
    if (!gTrackLoadInfo.collision.loading) {
        gTrackLoadInfo.collision.idx = LoadWave(track + 139, 0, background, 0, 0);
        gTrackLoadInfo.collision.loading = 1;
    }
    if (!WaveInstanceReady(gTrackLoadInfo.collision.idx)) {
        return 0;
    }
    gTrackLoadInfo.collision.loaded = 1;
    gTrackLoadInfo.collision.inited = 1;
    if (!gTrackLoadInfo.collision.ready) {
        gRoadDat = GetWaveInstanceAddr(gTrackLoadInfo.collision.idx);
        gTrackLoadInfo.collision.ready = 1;
    }
    if (!gTrackLoadInfo.path.loading) {
        gTrackLoadInfo.path.idx = LoadWave((gBackwards ? 177 : 158) + track, 0, background, 0, 0);
        gTrackLoadInfo.path.loading = 1;
    }
    if (!WaveInstanceReady(gTrackLoadInfo.path.idx)) {
        return 0;
    }
    gTrackLoadInfo.path.loaded = 1;
    gTrackLoadInfo.path.inited = 1;
    if (!gTrackLoadInfo.path.ready) {
        gPathWave = GetWaveInstanceAddr(gTrackLoadInfo.path.idx);
        gTrackLoadInfo.path.ready = 1;
    }
    if (!gTrackLoadInfo.other.loading) {
        gTrackLoadInfo.other.loading = 1;
        if (!(gGameState & 0x100)) {
            if (gRace == 6 || (track >= 6 && track < 14)) {
                LoadWave(76, 0, 0, 0, 0);
            }
            if (!(gGameState & 8) || ((gGameState & 8) && expansion_pak_present)) {
                LoadWave(68, 0, 0, 0, 0);
                if (track == 18) {
                    LoadWave(78, 0, 0, 0, 0);
                } else if (track >= 0 && track < 6) {
                    if (expansion_pak_present) {
                        LoadWave(78, 0, 0, 0, 0);
                        LoadWave(gTrackNum + 82, 0, 0, 0, 0);
                    } else {
                        LoadWave(79, 0, 0, 0, 0);
                    }
                }
                LoadWave(77, 0, 0, 0, 0);
            }
            LoadWave(62, 0, 0, 0, 0);
            if (expansion_pak_present || gTrackNum >= 6) {
                LoadWave(80, 0, 0, 0, 0);
                LoadWave(61, 0, 0, 0, 0);
            } else {
                LoadWave(81, 0, 0, 0, 0);
            }
            LoadWave(64, 0, 0, 0, 0);
            if (gNumWindows >= 2) {
                LoadWave(66, 0, 0, 0, 0);
                if (expansion_pak_present || gTrackNum >= 6) {
                    LoadWave(65, 0, 0, 0, 0);
                }
            }
            if (gRace == 6) {
                LoadWave(63, 0, 0, 0, 0);
            } else if (gRace == 4) {
                LoadWave(67, 0, 0, 0, 0);
            }
            if (gGameState & 8) {
            } else if (gGameState & 8) {
            }
            if (gGameState & 8) {
            }
        }
        gTrackLoadInfo.other.loaded = 1;
        gTrackLoadInfo.other.ready = 1;
        gTrackLoadInfo.other.inited = 1;
    }
    if (gTrackLoadInfo.trackInited) {
        return 1;
    }
    gTrackLoadInfo.trackInited = 1;
    initTargetStructs(track);
    SkyInit();
    NewWorld((u8 *)gWorldNames[track], -1);
    InitEnvs();
    InitTrackDynamicObjs();
    if (!(gGameState & 0x100) && !reloading) {
        LoadCars(0);
    }
    InitVisuals(!(gGameState & 0x100));
    InitCheckpointData();
    InitPathData();
    if (gTrackNum >= 0 && gTrackNum < 6) {
        InitDemoPathPoint();
    }
    InitSkids();
    InitSmoke();
    InitBlast();
    if (expansion_pak_present || track >= 6) {
        initWorldDamage();
    }
    return 1;
}
