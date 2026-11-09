/* Reading a wave from the cartridge and setting it up (MAT3 waves.c _ReadWave, _SetupWaveModel): a wave is stored
 * raw, LZSS-compressed or deflated (wave_storage_kinds); a model wave is a WAD whose model, texture and palette
 * tables go into gRomObjDefPtrs, gTexDefPtrs and gPalDefPtrs, with their addresses rebased and their display lists
 * relocated (waves/group.c has the waves). */
#include "game/game.h"
#define OSIoMesg OSIoMesgView   /* this file's own OSIoMesg: include/game's OSIoMesg is another type */
#define OSIoMesgHdr OSIoMesgHdrView   /* this file's own OSIoMesgHdr: include/game's OSIoMesgHdr is another type */
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue { void *mtqueue; void *fullqueue; s32 validCount; s32 first; s32 msgCount; OSMesg *msg; } OSMesgQueue;
typedef struct OSIoMesgHdr { u16 type; u8 pri; u8 status; OSMesgQueue *retQueue; } OSIoMesgHdr;
typedef struct OSIoMesg { OSIoMesgHdr hdr; void *dramAddr; u32 devAddr; u32 size; void *piHandle; } OSIoMesg;

/* A handle wave's ptr is a heap master slot, a u32 window address (the heap's handle tables keep N64 layout). */
#define HANDLE_BLOCK(h) ((u8 *)PC_N64P(PC_SLOT_N64(h)))

typedef struct tLod {               /* N64 0x10 (GC anon_texidx_flags_c 0xc) */
    u16 texidx;
    u16 flags;
    f32 lodRad;
    WADP(u32) displayList;          /* 0x8 */
    WADP(u32) vtxList;              /* 0xc N64 only, relocated with the display list */
} tLod;
typedef struct tModelDef {          /* N64 0x58 */
    char name[16];
    f32 radius;
    s16 unused;
    s16 lodCount;                   /* 0x16 */
    tLod lod[4];                    /* 0x18 */
} tModelDef;
typedef struct tTexture {           /* N64 0x24 */
    char name[16];
    u16 width;
    u16 height;
    u8 size;
    u8 format;
    s16 palIndex;
    WADP(char) texBase;             /* 0x18 */
    u32 flags;
    u32 xflags;
} tTexture;
typedef struct anon_name_start_18 { /* palette, 0x18 */
    char name[16];
    u8 start;
    u8 end;
    u16 format;
    WADP(char) palBase;             /* 0x14 */
} anon_name_start_18;
typedef struct tBasicTargetPath {   /* 0x24 */
    char name[16];
    u32 flags;
    s16 numPts;
    s16 collIdx;
    WADP(char) trigPtr;             /* 0x18 */
    WADP(char) pts;                 /* 0x1c */
    WADP(void) parent;
} tBasicTargetPath;
typedef struct anon_romObjDefs_count_8 { tModelDef *romObjDefs; s32 count; } anon_romObjDefs_count_8;
typedef struct anon_texDefs_count_8 { tTexture *texDefs; s32 count; } anon_texDefs_count_8;
typedef struct anon_palDefs_count_8 { anon_name_start_18 *palDefs; s32 count; } anon_palDefs_count_8;
typedef struct anon_paths_count_8 { tBasicTargetPath *paths; s32 count; } anon_paths_count_8;

VIEW(anon_romObjDefs_count_8[], gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view
VIEW(anon_texDefs_count_8[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view
VIEW(anon_palDefs_count_8[], gPalDefPtrs);
#define gPalDefPtrs gPalDefPtrs_view
VIEW(anon_paths_count_8, gTargetPaths);
#define gTargetPaths gTargetPaths_view

VIEW(s32[], gWaveSizes);
#define gWaveSizes gWaveSizes_view
VIEW(OSMesgQueue, rom_dma_queue);
#define rom_dma_queue rom_dma_queue_view

/* A model WAD's tables, by tag. */
#define WAD_TAG_OBHD 0x4F424844     /* 'OBHD': the models (tModelDef) */
#define WAD_TAG_TXHD 0x54584844     /* 'TXHD': the textures (tTexture) */
#define WAD_TAG_PLHD 0x504C4844     /* 'PLHD': the palettes */
#define WAD_TAG_PTHD 0x50544844     /* 'PTHD': a track's target paths (tBasicTargetPath) */
#define WAD_TAG_TXLD 0x54584C44     /* 'TXLD': the texture load commands */
#define WAD_TAG_IMAG 0x494D4147     /* 'IMAG': the texels and palettes */
/* The waves that hold a track's target paths. */
#define WAVE_TRACK_PATHS_FIRST 101
#define WAVE_TRACK_PATHS_END 120
/* The sections of a model's display lists relocate_model_dl keeps (its markers name the case each is for). */
#define DL_KEEP_ONE_PLAYER 2
#define DL_KEEP_TWO_PLAYERS 4
#define DL_KEEP_FORWARD 8           /* the track run the usual way */
#define DL_KEEP_BACKWARD 0x10       /* the track run backwards */

#define MBGetFromWad(wad, tag, count) ((uintptr_t)MBGetFromWad(wad, tag, count))
/* relocate_texture_loads walks TXLD from start to end as Gfx pointers. */
void relocate_texture_loads(void *start, void *end, s32 IMAGptr);
#define relocate_texture_loads(start, end, IMAGptr) \
    relocate_texture_loads((void *)PC_N64P((u32)(start)), (void *)PC_N64P((u32)(end)), IMAGptr)

void osInvalICache(void *vaddr, s32 nbytes);
void osInvalDCache(void *vaddr, s32 nbytes);
s32 osPiStartDma(OSIoMesg *mb, s32 priority, s32 direction, u32 devAddr, void *vAddr, u32 nbytes, OSMesgQueue *mq);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
void lzss_decompress(s32 src, void *dst);
void inflate_decompress(s32 src, void *dst, s32 flag);

/* Sets up wave instance idx once its file is in (MAT3 _SetupWaveModel): for a model WAD, fills the instance's
 * entries of the model, texture, palette (and, for a track wave, target path) tables, rebases the textures' and
 * palettes' data to the WAD's IMAG block and relocates the texture loads; the first time (first set) also rebases
 * each level of detail's display and vertex lists and relocates the display lists, keeping the sections for this
 * race's player count and direction (and mirroring them for a mirrored track). With first clear, after the heap has
 * moved a handle wave, the tables are rebased by how far it moved. Then the instance is ready, and mirrored to the
 * current direction.
 * PC: video.detail high keeps the one-player sections of the models' display lists in split screen too (the LOD
 * markers relocate_model_dl rewrites: docs/port/DETAIL.md 2.5); PC_DETAIL_PLAYERS is pc_game_types.h's. */
void SetupWaveModel(s32 idx, s32 first) {
    WadHandle wad;
    u8 *WADptr;
    s32 TXLDptr;
    s32 TXLDcnt;
    s32 IMAGptr;
    u8 *offset;
    s32 i;
    s32 j;
    s32 modelIdx;
    s32 flags;

    flags = 0;
    if (gWaveInstance[idx].model == 0) {
        gWaveInstance[idx].ready = 1;
        return;
    }
    if (first == 0 && gWaveInstance[idx].handle == 0) {
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
    if (first != 0) {
        MBSetupWad(&wad, WADptr);
    } else {
        offset = HANDLE_BLOCK(gWaveInstance[idx].ptr) - PC_N64A(gWaveInstance[idx].lastPtr);
        MBOffsetWad(&wad, (s32)PC_N64A(WADptr), (s32)PC_N64A(offset));
    }
    gWaveInstance[idx].lastPtr = HANDLE_BLOCK(gWaveInstance[idx].ptr);
    gRomObjDefPtrs[modelIdx].romObjDefs = ((tModelDef *)PC_N64P(MBGetFromWad(&wad, WAD_TAG_OBHD, &gRomObjDefPtrs[modelIdx].count)));
    gTexDefPtrs[modelIdx].texDefs = ((tTexture *)PC_N64P(MBGetFromWad(&wad, WAD_TAG_TXHD, &gTexDefPtrs[modelIdx].count)));
    gPalDefPtrs[modelIdx].palDefs = ((anon_name_start_18 *)PC_N64P(MBGetFromWad(&wad, WAD_TAG_PLHD, &gPalDefPtrs[modelIdx].count)));
    if (gWaveInstance[idx].wave >= WAVE_TRACK_PATHS_FIRST && gWaveInstance[idx].wave < WAVE_TRACK_PATHS_END) {
        gTargetPaths.paths = ((tBasicTargetPath *)PC_N64P(MBGetFromWad(&wad, WAD_TAG_PTHD, &gTargetPaths.count)));
    }
    TXLDptr = MBGetFromWad(&wad, WAD_TAG_TXLD, &TXLDcnt);
    IMAGptr = MBGetFromWad(&wad, WAD_TAG_IMAG, NULL);
    relocate_texture_loads(TXLDptr, TXLDptr + TXLDcnt, IMAGptr);
    for (i = 0; i < gTexDefPtrs[modelIdx].count; i++) {
        gTexDefPtrs[modelIdx].texDefs[i].texBase += IMAGptr;
    }
    for (i = 0; i < gPalDefPtrs[modelIdx].count; i++) {
        gPalDefPtrs[modelIdx].palDefs[i].palBase += IMAGptr;
    }
    if (first != 0) {
        for (i = 0; i < gRomObjDefPtrs[modelIdx].count; i++) {
            for (j = 0; j < gRomObjDefPtrs[modelIdx].romObjDefs[i].lodCount; j++) {
                gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].displayList = (WADP(u32))((u32)gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].displayList + PC_N64A(WADptr));
                gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].vtxList = (WADP(u32))((u32)gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].vtxList + PC_N64A(WADptr));
                relocate_model_dl(((u32 *)PC_N64P(gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].displayList)), (s32)PC_N64A(WADptr), -1, gMirrorWaves ? 1 : 0, flags);
            }
        }
    }
    if (gWaveInstance[idx].wave >= WAVE_TRACK_PATHS_FIRST && gWaveInstance[idx].wave < WAVE_TRACK_PATHS_END) {
        for (i = 0; i < gTargetPaths.count; i++) {
            gTargetPaths.paths[i].pts = gTargetPaths.paths[i].pts + PC_N64A((first ? WADptr : offset));
            gTargetPaths.paths[i].trigPtr = gTargetPaths.paths[i].trigPtr + PC_N64A((first ? WADptr : offset));
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

/* Sets up a handle wave again after the heap moved it: its tables are rebased by the distance (SetupWaveModel with
 * first = 0). */
void relocate_wave_model(s32 idx) {
    SetupWaveModel(idx, 0);
}
