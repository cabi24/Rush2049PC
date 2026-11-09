/* LoadTrack: a track's files loaded and the track set up (MAT3 tracks.c). */
#include "game/game.h"

/* Loads track `track` a step at a time (`background`: the loads go to the background loader), each part's state in
 * gTrackLoadInfo: its track wave (101 + track), its world WAD (120 + track: the world headers, the target groups),
 * its collision WAD (139 + track: gRoadDat), its path file (158 + track, 177 + track backwards: gPathWave), then
 * the race's other waves (by race kind, track, the Expansion Pak and the windows; none in state 0x100). Returns 0
 * while a part is still loading. Once all are in, sets the track up once (unless already): targets, sky, world,
 * environment, dynamic objects, the cars (but on a reload or in state 0x100), visuals, checkpoints, paths, the demo
 * path point, skids, smoke, blasts and world damage. Then returns 1. */
s32 LoadTrack(s32 track, s32 background, s32 reloading) {
    struct pc_world_records *world;

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
        /* MAT3 finds the world's chunks through the WAD's table of contents (MBSetupWad, MBGetFromWad: WHDR, GDAT,
         * GTLD) and, the first time, adds the file's address to each world tree's offset in WHDR. On the PC the
         * world's converter has built those as records beside the file when it loaded (port/assets/world.c): the
         * names with each root a host pointer, GDAT where it is in the file, GTLD's keys with room for their
         * targets. */
        world = pc_world_records(GetWaveInstanceAddr(gTrackLoadInfo.world.idx));
        gWorldData = &world->numWorlds;
        WorldTable = world->worlds;
        gTrackLoadInfo.world.inited = 1;
        gTargetGroups = world->targetGroups;
        numgTargetGroups = world->numTargetGroups;
        gTargetGroupList = world->targetGroupList;
        numgTargetGroupList = world->numTargetGroupList;
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
        if (!(gGameState & GS_CAR)) {
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
    if (!(gGameState & GS_CAR) && !reloading) {
        LoadCars(0);
    }
    InitVisuals(!(gGameState & GS_CAR));
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
