/* Reading a wave from the cartridge and setting it up (MAT3 waves.c _ReadWave, _SetupWaveModel): a wave is stored
 * raw, LZSS-compressed or deflated (wave_storage_kinds); a model wave is a WAD whose model, texture and palette
 * records (built beside the file by its converter, port/assets/wad.c) go into gRomObjDefPtrs, gTexDefPtrs and
 * gPalDefPtrs, and its display lists are relocated (waves/group.c has the waves). */
#include "game/game.h"
#define OSIoMesg OSIoMesgView   /* this file's own OSIoMesg: include/game's OSIoMesg is another type */
#define OSIoMesgHdr OSIoMesgHdrView   /* this file's own OSIoMesgHdr: include/game's OSIoMesgHdr is another type */
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue { void *mtqueue; void *fullqueue; s32 validCount; s32 first; s32 msgCount; OSMesg *msg; } OSMesgQueue;
typedef struct OSIoMesgHdr { u16 type; u8 pri; u8 status; OSMesgQueue *retQueue; } OSIoMesgHdr;
typedef struct OSIoMesg { OSIoMesgHdr hdr; void *dramAddr; u32 devAddr; u32 size; void *piHandle; } OSIoMesg;

/* A handle wave's ptr is a heap master slot (a handle): its block is the address the slot holds. */
#define HANDLE_BLOCK(h) (*(u8 **)(h))

/* The waves that hold a track's target paths. */
#define WAVE_TRACK_PATHS_FIRST 101
#define WAVE_TRACK_PATHS_END 120
/* The sections of a model's display lists relocate_model_dl keeps (its markers name the case each is for). */
#define DL_KEEP_ONE_PLAYER 2
#define DL_KEEP_TWO_PLAYERS 4
#define DL_KEEP_FORWARD 8           /* the track run the usual way */
#define DL_KEEP_BACKWARD 0x10       /* the track run backwards */

void osInvalICache(void *vaddr, s32 nbytes);
void osInvalDCache(void *vaddr, s32 nbytes);
s32 osPiStartDma(OSIoMesg *mb, s32 priority, s32 direction, u32 devAddr, void *vAddr, u32 nbytes, OSMesgQueue *mq);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);

/* Sets up wave instance idx once its file is in (MAT3 _SetupWaveModel): for a model WAD, points the instance's
 * entries of the model, texture and palette tables (and, for a track wave, the target paths) at the WAD's records,
 * relocates its texture loads and its display lists, keeping the sections for this race's player count and
 * direction (and mirroring them for a mirrored track). Then the instance is ready, and mirrored to the current
 * direction. With first clear MAT3 set up a handle wave the heap had moved again; no caller asks for that (a model
 * WAD is never a handle: the font data is, which has no models).
 * PC: the records are the WAD's converter's, built beside the file when it loaded, with host pointers into it
 * (pc_wad_records, port/assets/wad.c). On the N64 this rebased the file's own records in place: the table of contents
 * (MBSetupWad), every record's offsets by the file's or the IMAG chunk's address (texBase, palBase, displayList,
 * vtxList, pts, trigPtr), and a moved handle wave's by how far it had moved (MBOffsetWad). The display lists and
 * texture loads keep the N64's form and are relocated in place by the N64 addresses of the file and of its IMAG chunk
 * (docs/port/NATIVE.md step 10). video.detail high keeps the one-player sections of the models' display lists in
 * split screen too (the LOD markers relocate_model_dl rewrites: docs/port/DETAIL.md 2.5); PC_DETAIL_PLAYERS is
 * pc_game_types.h's. */
void SetupWaveModel(s32 idx, s32 first) {
    struct pc_wad_records *wad;
    u8 *WADptr;
    s32 n64File;
    s32 i;
    s32 j;
    s32 modelIdx;
    s32 flags;

    flags = 0;
    if (gWaveInstance[idx].model == 0) {
        gWaveInstance[idx].ready = 1;
        return;
    }
    if (first == 0) {
        return;
    }
    if (PC_DETAIL_PLAYERS(gNumPlayers) == 1) {
        flags = DL_KEEP_ONE_PLAYER;
    } else if (gNumPlayers == 2) {
        flags = DL_KEEP_TWO_PLAYERS;
    }
    if (gBackwards != 0) {
        flags |= DL_KEEP_BACKWARD;
    } else {
        flags |= DL_KEEP_FORWARD;
    }
    if (gWaveInstance[idx].handle != 0) {
        WADptr = HANDLE_BLOCK(gWaveInstance[idx].ptr);
    } else {
        WADptr = gWaveInstance[idx].ptr;
    }
    modelIdx = idx;
    wad = pc_wad_records(WADptr);
    gRomObjDefPtrs[modelIdx].romObjDefs = wad->models;
    gRomObjDefPtrs[modelIdx].count = wad->numModels;
    gTexDefPtrs[modelIdx].texDefs = wad->textures;
    gTexDefPtrs[modelIdx].count = wad->numTextures;
    gPalDefPtrs[modelIdx].palDefs = wad->palettes;
    gPalDefPtrs[modelIdx].count = wad->numPalettes;
    if (gWaveInstance[idx].wave >= WAVE_TRACK_PATHS_FIRST && gWaveInstance[idx].wave < WAVE_TRACK_PATHS_END) {
        gTargetPaths.paths = wad->paths;
        gTargetPaths.count = wad->numPaths;
    }
    if (wad->textureLoads != NULL) {
        relocate_texture_loads(wad->textureLoads, (struct N64Gfx *)((u8 *)wad->textureLoads + wad->textureLoadsSize),
                               PC_N64A(wad->images));
    }
    n64File = PC_N64A(WADptr);
    for (i = 0; i < gRomObjDefPtrs[modelIdx].count; i++) {
        for (j = 0; j < gRomObjDefPtrs[modelIdx].romObjDefs[i].lodCount; j++) {
            relocate_model_dl(gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].displayList, n64File, -1, gMirrorWaves ? 1 : 0, flags);
        }
    }
    gWaveInstance[idx].ready = 1;
    MirrorWaveInstance(idx);
}

/* _ReadWave's SetupWaveModel(slot, 1): convert the file to the host's byte order first (port/assets). The
 * re-setup after a heap move (first = 0) converts nothing. */
#define SetupWaveModel(slot, first) ((first) ? pc_wave_loaded(gWaveInstance[slot].wave, gWaveInstance[slot].handle ? HANDLE_BLOCK(gWaveInstance[slot].ptr) : gWaveInstance[slot].ptr, gWaveInstance[slot].size) : (void)0, SetupWaveModel(slot, first))

/* Reads size bytes at cartridge offset rom into dst by PI DMA, and waits for it. */
void wave_rom_read(u8 *dst, s32 rom, s32 size) {
    OSIoMesg mb;

    osInvalICache(dst, size);
    osInvalDCache(dst, size);
    osPiStartDma(&mb, 0, 0, rom, dst, size, &rom_dma_queue);
    osRecvMesg(&rom_dma_queue, NULL, 1);
}

/* Reads wave instance slot's wave from the cartridge into its block, as the wave is stored (wave_storage_kinds: 0
 * raw, 1 LZSS, else deflate), then sets it up (MAT3 _ReadWave). Runs on the game thread or the background loader. */
void ReadWave(s32 slot) {
    anon_loaded_ready_14 *r;
    u8 *dst;
    s32 size;
    s32 rom;
    u8 type;

    r = &gWaveInstance[slot];
    type = r->wave;
    if (wave_storage_kinds[type] == 0) {
        if (r->handle != 0) {
            dst = HANDLE_BLOCK(r->ptr);
        } else {
            dst = r->ptr;
        }
        rom = wave_rom_offsets[type];
        size = gWaveSizes[type];
        wave_rom_read(dst, rom, size);
    } else if (wave_storage_kinds[type] == 1) {
        if (r->handle != 0) {
            dst = HANDLE_BLOCK(r->ptr);
        } else {
            dst = r->ptr;
        }
        lzss_decompress(wave_rom_offsets[type], dst);
    } else {
        if (r->handle != 0) {
            dst = HANDLE_BLOCK(r->ptr);
        } else {
            dst = r->ptr;
        }
        inflate_decompress(wave_rom_offsets[type], dst, 1);
    }
    r->loaded = 1;
    SetupWaveModel(slot, 1);
}

