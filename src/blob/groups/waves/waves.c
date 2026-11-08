/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * waves.c: _SetupWaveModel (SetupWaveModel 0x80096CA8, 1,204 B), the inlined DMA-read helper
 * (wave_rom_read, deleted stub 0x8009715C), _ReadWave (ReadWave 0x80097164, 344 B) and the handle-wave re-setup
 * (relocate_wave_model 0x800AC6F4, 104 B).
 *
 * Provenance: MAT3 GameSource/src/waves.c _SetupWaveModel (GC 0x8005BF8C) / _ReadWave (GC 0x8005BE10) for the
 * statement order, names and types; Dreamcast src/waves.c func_8C06588C (= _SetupWaveModel; DC status `differs`,
 * used as a structural lead) for the `modelIdx` copy: every gRomObjDefPtrs/gTexDefPtrs/gPalDefPtrs index goes
 * through modelIdx, gWaveInstance through idx. uopt copy-propagates modelIdx -> idx outside the loop back-edges and
 * counts every propagated occurrence toward idx's colour priority: that is what gives retail's IPA convention
 * (idx in s8, first in t0) and the `sw s8,68(sp)` / reloaded bound of the model loops. Dreamcast func_8C065862 is
 * relocate_wave_model (re-relocates a handle wave after the heap moved it: first = 0).
 * Builds on (resource-loader drafts: offsets, the uninitialised `offset` read on the first path,
 * tables as {ptr,count} pairs, gTargetPaths as one struct, `x = x + (first ? WADptr : offset)` fixups).
 * N64 differences: no byte swapping (big-endian), no AddTexture/RegisterObjName/strstr arms; the model display lists
 * are walked by relocate_model_dl (N64 DL relocation) with render flags computed up front (player count, gBackwards)
 * and gMirrorWaves (gMirrorWaves); texture and palette data pointers are relocated by IMAGptr; target-path waves are
 * 101..119; _ReadWave loads raw ROM (DMA), lzss or inflate data per wave.
 * DISCLOSED SHAPING DEVICE: the empty `do {} while (0);` between the texture and palette loops (an empty debug-macro
 * expansion, as in the locked ZOID_AddBlit) ends a uopt basic block: it lengthens IMAGptr's and offset's live ranges
 * (colour divisors 3->4 and 14->15) so the gTexDefPtrs address outranks IMAGptr (s0/s1) and i*88 outranks offset
 * (s4/s5) as in retail; force.sh proved those two inversions were the whole residual. `if (IMAGptr) {}` at the same
 * place is equivalent. No unused locals, no pad, no volatile.
 * wave_rom_read: the caller-less locked 8-byte stub between SetupWaveModel and _ReadWave is this inlined static. Its
 * inline site is _ReadWave's frame (112 exact; needed three unused locals) and, defined before _ReadWave, its
 * lower .loc lines give as1's tie-break retail's `move a0,s0` before `lw s3`.
 * All are private IPA callees (SetupWaveModel: idx s8, first t0; _ReadWave: idx s8): internal, not on keep.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
#define NULL ((void *)0)

typedef void *OSMesg;
typedef struct OSMesgQueue { void *mtqueue; void *fullqueue; s32 validCount; s32 first; s32 msgCount; OSMesg *msg; } OSMesgQueue;
typedef struct OSIoMesgHdr { u16 type; u8 pri; u8 status; OSMesgQueue *retQueue; } OSIoMesgHdr;
typedef struct OSIoMesg { OSIoMesgHdr hdr; void *dramAddr; u32 devAddr; u32 size; void *piHandle; } OSIoMesg;

typedef struct anon_loaded_ready_14 {   /* gWaveInstance[64], 0x14 */
    signed char loaded;     /* 0x0 */
    signed char ready;      /* 0x1 */
    signed char locked;     /* 0x2 */
    signed char model;      /* 0x3 */
    unsigned char mirrored; /* 0x4 */
    signed char handle;     /* 0x5 */
    unsigned char wave;     /* 0x6 (u16 on GC) */
    unsigned int size;      /* 0x8 */
    unsigned char *ptr;     /* 0xc */
    unsigned char *lastPtr; /* 0x10 */
} anon_loaded_ready_14;

typedef struct anon_base_TOCentries_c { u32 *base; s32 TOCentries; void *TOC; } anon_base_TOCentries_c; /* mb_wad.c */

/* The loaded records keep their N64 layout on PC (port/assets/wad.c swaps their scalars in place): their pointer
 * fields are 32-bit RDRAM-window addresses, which widen to host pointers (docs/port/ASSETS.md section 5). */
#define WADP(type) u32
/* A handle wave's ptr is a heap master slot, a u32 window address (the heap's handle tables keep N64 layout). */
#define HANDLE_BLOCK(h) ((u8 *)PC_N64P(PC_SLOT_N64(h)))
/* The relocation's base and offset, and the records' addresses, are 32-bit N64 addresses: N64A(p) is a window
 * pointer's, N64PTR(T, a) the pointer at one (pc_prelude.h, docs/port/NOFIXED.md 2.3). An offset between two blocks
 * (a pointer less an N64 address) translates to the difference. */
#define N64A(x) PC_N64A(x)
#define N64AS(x) (s32)PC_N64A(x)
#define N64PTR(T, a) ((T)PC_N64P(a))
#define N64PTR0(T, a) ((T)PC_N64P(a))

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

extern anon_loaded_ready_14 gWaveInstance[];        /* 0x80156D38 */
extern anon_romObjDefs_count_8 gRomObjDefPtrs[];    /* 0x801161F4 */
extern anon_texDefs_count_8 gTexDefPtrs[];          /* 0x80151AE8 */
extern anon_palDefs_count_8 gPalDefPtrs[];          /* 0x80138670 */
extern anon_paths_count_8 gTargetPaths;               /* gTargetPaths */
extern s16 gNumPlayers;                     /* gNumPlayers */
extern s8 gBackwards;                               /* render mode flag: DL flags 0x10 / 0x08 */
extern s8 gMirrorWaves;                               /* relocate_model_dl flag */
extern s32 wave_storage_kinds[];                            /* per-wave storage kind: 0 raw ROM, 1 lzss, 2 inflate */
extern s32 wave_rom_offsets[];                            /* per-wave ROM address */
extern s32 gWaveSizes[];                            /* gWaveSizes */
extern OSMesgQueue rom_dma_queue;                      /* PI DMA completion queue */

void MBSetupWad(anon_base_TOCentries_c *wad, u8 *WADptr);              /* MBSetupWad */
void MBOffsetWad(anon_base_TOCentries_c *wad, s32 WADptr, s32 offset);  /* MBOffsetWad */
/* A TOC entry's address is a u32 window address: zero-extended, not sign-extended, into a pointer. */
u32 MBGetFromWad(anon_base_TOCentries_c *wad, s32 tag, s32 *count);
#define MBGetFromWad(wad, tag, count) ((uintptr_t)MBGetFromWad(wad, tag, count))
/* relocate_texture_loads walks TXLD from start to end as Gfx pointers. */
void relocate_texture_loads(void *start, void *end, s32 IMAGptr);
#define relocate_texture_loads(start, end, IMAGptr) \
    relocate_texture_loads((void *)PC_N64P((u32)(start)), (void *)PC_N64P((u32)(end)), IMAGptr)
void relocate_model_dl(u32 *dl, s32 base, s32 seg, s32 flag, u32 mask);
void MirrorWaveInstance(s32 idx);                                         /* MirrorWaveInstance */
void osInvalICache(void *vaddr, s32 nbytes);
void osInvalDCache(void *vaddr, s32 nbytes);
s32 osPiStartDma(OSIoMesg *mb, s32 priority, s32 direction, u32 devAddr, void *vAddr, u32 nbytes, OSMesgQueue *mq);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
void lzss_decompress(s32 src, void *dst);
void inflate_decompress(s32 src, void *dst, s32 flag);

/* PC: video.detail high keeps the one-player sections of the models' display lists in split screen too (the LOD
 * markers relocate_model_dl rewrites: docs/port/DETAIL.md 2.5); PC_DETAIL_PLAYERS is pc_game_types.h's. */
void SetupWaveModel(s32 idx, s32 first) {
    anon_base_TOCentries_c wad;
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
        flags = 2;
    } else if (gNumPlayers == 2) {
        flags = 4;
    }
    if (gBackwards != 0) {
        flags |= 0x10;
    } else {
        flags |= 8;
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
        offset = HANDLE_BLOCK(gWaveInstance[idx].ptr) - N64A(gWaveInstance[idx].lastPtr);
        MBOffsetWad(&wad, N64AS(WADptr), N64AS(offset));
    }
    gWaveInstance[idx].lastPtr = HANDLE_BLOCK(gWaveInstance[idx].ptr);
    gRomObjDefPtrs[modelIdx].romObjDefs = N64PTR(tModelDef *, MBGetFromWad(&wad, 0x4F424844, &gRomObjDefPtrs[modelIdx].count));
    gTexDefPtrs[modelIdx].texDefs = N64PTR(tTexture *, MBGetFromWad(&wad, 0x54584844, &gTexDefPtrs[modelIdx].count));
    gPalDefPtrs[modelIdx].palDefs = N64PTR(anon_name_start_18 *, MBGetFromWad(&wad, 0x504C4844, &gPalDefPtrs[modelIdx].count));
    if (gWaveInstance[idx].wave >= 101 && gWaveInstance[idx].wave < 120) {
        gTargetPaths.paths = N64PTR(tBasicTargetPath *, MBGetFromWad(&wad, 0x50544844, &gTargetPaths.count));
    }
    TXLDptr = MBGetFromWad(&wad, 0x54584C44, &TXLDcnt);
    IMAGptr = MBGetFromWad(&wad, 0x494D4147, NULL);
    relocate_texture_loads(TXLDptr, TXLDptr + TXLDcnt, IMAGptr);
    for (i = 0; i < gTexDefPtrs[modelIdx].count; i++) {
        gTexDefPtrs[modelIdx].texDefs[i].texBase += IMAGptr;
    }
    do {
    } while (0);
    for (i = 0; i < gPalDefPtrs[modelIdx].count; i++) {
        gPalDefPtrs[modelIdx].palDefs[i].palBase += IMAGptr;
    }
    if (first != 0) {
        for (i = 0; i < gRomObjDefPtrs[modelIdx].count; i++) {
            for (j = 0; j < gRomObjDefPtrs[modelIdx].romObjDefs[i].lodCount; j++) {
                gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].displayList = (WADP(u32))((u32)gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].displayList + N64A((first ? WADptr : offset)));
                gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].vtxList = (WADP(u32))((u32)gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].vtxList + N64A((first ? WADptr : offset)));
                relocate_model_dl(N64PTR0(u32 *, gRomObjDefPtrs[modelIdx].romObjDefs[i].lod[j].displayList), N64AS((first ? WADptr : offset)), -1, gMirrorWaves ? 1 : 0, flags);
            }
        }
    }
    if (gWaveInstance[idx].wave >= 101 && gWaveInstance[idx].wave < 120) {
        for (i = 0; i < gTargetPaths.count; i++) {
            gTargetPaths.paths[i].pts = gTargetPaths.paths[i].pts + N64A((first ? WADptr : offset));
            gTargetPaths.paths[i].trigPtr = gTargetPaths.paths[i].trigPtr + N64A((first ? WADptr : offset));
        }
    }
    gWaveInstance[idx].ready = 1;
    MirrorWaveInstance(idx);
}

/* _ReadWave's SetupWaveModel(slot, 1): convert the file to the host's byte order first (port/assets). The
 * re-setup after a heap move (first = 0) converts nothing. */
#define SetupWaveModel(slot, first) ((first) ? pc_wave_loaded(gWaveInstance[slot].wave, gWaveInstance[slot].handle ? HANDLE_BLOCK(gWaveInstance[slot].ptr) : gWaveInstance[slot].ptr, gWaveInstance[slot].size) : (void)0, SetupWaveModel(slot, first))

void wave_rom_read(u8 *dst, s32 rom, s32 size) {
    OSIoMesg mb;

    osInvalICache(dst, size);
    osInvalDCache(dst, size);
    osPiStartDma(&mb, 0, 0, rom, dst, size, &rom_dma_queue);
    osRecvMesg(&rom_dma_queue, NULL, 1);
}

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

/* Re-relocates a handle wave after the heap moved it (Dreamcast func_8C065862; no GC counterpart):
 * `first` = 0, so _SetupWaveModel applies the offset path. */
void relocate_wave_model(s32 idx) {
    SetupWaveModel(idx, 0);
}
