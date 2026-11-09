/* LoadTrack: a track's files loaded and the track set up (MAT3 tracks.c). */
#include "game/game.h"

/* A world group's header (MAT3 world.c's anon_woff_wname_14): its offset in the world WAD and its name. */
typedef struct WorldHdr {
    u32 woff;                  /* 0x00 */
    char wname[16];            /* 0x04 */
} WorldHdr;
/* A WAD being read (MAT3 mb_wad.c's anon_base_TOCentries_c). */
typedef struct Wad {
    u32 *base;
    s32 TOCentries;
    void *TOC;
} Wad;

/* PC: the GTLD keys become object pointers at race setup (SetTargetDefs), which don't fit the file's 4-byte
 * slots, so LoadTrack gets GTLD as a native array of the keys (port/assets/world.c); other chunks are returned
 * as they are. */
uintptr_t pc_world_chunk(u32 addr, s32 tag, s32 *count);
#define MBGetFromWad(wad, tag, count) pc_world_chunk(MBGetFromWad(wad, tag, count), tag, count)

/* The world headers' offsets become 32-bit N64 addresses: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
/* Loads track `track` a step at a time (`background`: the loads go to the background loader), each part's state in
 * gTrackLoadInfo: its track wave (101 + track), its world WAD (120 + track: the world headers, the target groups),
 * its collision WAD (139 + track: gRoadDat), its path file (158 + track, 177 + track backwards: gPathWave), then
 * the race's other waves (by race kind, track, the Expansion Pak and the windows; none in state 0x100). Returns 0
 * while a part is still loading. Once all are in, sets the track up once (unless already): targets, sky, world,
 * environment, dynamic objects, the cars (but on a reload or in state 0x100), visuals, checkpoints, paths, the demo
 * path point, skids, smoke, blasts and world damage. Then returns 1. */
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
            if (gRace == kBattle || (track >= 6 && track < 14)) {
                LoadWave(76, 0, 0, 0, 0);
            }
            if (!(gGameState & GS_DEMO) || ((gGameState & GS_DEMO) && expansion_pak_present)) {
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
            if (gRace == kBattle) {
                LoadWave(63, 0, 0, 0, 0);
            } else if (gRace == kStunt) {
                LoadWave(67, 0, 0, 0, 0);
            }
            /* the N64 code tests the demo state here three times and does nothing */
            if (gGameState & GS_DEMO) {
            } else if (gGameState & GS_DEMO) {
            }
            if (gGameState & GS_DEMO) {
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
