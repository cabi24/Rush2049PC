/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Controller Pak system (MAT3 paksysdc.c, N64 osPfs form). , superseded.
 * One file, GC paksysdc.c names:
 * PS_CreateFile (0x800A2D4C) - body, unchanged
 *   pak_insert_file (0x800A2D0C)           - one-parameter InsertLLE(&gPS_Files[psfile->pak], fileh, head) helper
 * RemovePak = _RemovePak - body (osSendMesg), private IPA (unsaved s-registers); MATCH
 *                                       (the group needs ctx_heap.c: DeallocateMemory's IPA a1/a2 convention)
 *   ClearPak = _ClearPak         - memset of the 772-byte gPaks record keeping .damaged; pak in a3 (IPA)
 * PS_PakServer = PS_PakServer (0x800A377C) - MATCH
 * PS_DeleteFile = PS_DeleteFile (0x800A2680) - MATCH; its inlined helpers pdf_free_blocks and pdf_release
 *                                       are retail's caller-less stubs 0x800A2678 / 0x800A2670 (locked singles, not claimed);
 *                                       DeallocateMemory as the heap group's three-level DeallocateMemory_stub (pdf_dealloc,
 *                                       pdf_heap_lock, pdf_heap_unlock). No device, no unused local.
 * PS_InitPakSys = PS_InitPakSys (0x800A4508) - MATCH: MAT3 indexed handle loop
 *     `for (i = 0; i < 64; i++) { gFileHandles[i].file = &gPsFiles[i]; InsertLLE(&gFree, &gFileHandles[i], head); }`
 * (the pointer do-while coloured the free-list/end address webs s3/s4 the wrong way round; the indexed
 *     loop lets uopt build the end bound from the induction variable, giving retail s4 list / s3 end).
 *
 * Provenance: MAT3 decomp/src/GameSource/src/paksysdc.c (GC/PS2 platform layer: names, statement order of
 * PS_PakServer/PS_InitPakSys, ClearLL/InitLL use) + rush_d_dwarf.c locals (i, j, changed[4], dup, same, psfileh);
 * N64 osPfs bodies reconstructed from retail with candidate.c
 * as the starting skeleton; (PS_CreateFile), (InitPakSys,
 * _ClearPak), (_RemovePak). Rumble field names from rumble_thread.
 *
 * Shaping in PS_PakServer (all source-level; then):
 *  - no pak/other pointer locals (volatile gPaks indexed directly, as PS_CreateFile);
 *  - removed-pak branch: `if (locked) goto rj; if (!flush) { if (flushing) goto rj; _RemovePak; _ClearPak; } rj:`.
 *    Retail's CFG has the flush exit and the call path joining in a block of their own before the
 *    `damaged = 0; changed[i] = 1` block (the &changed[i] reload sits there), while the locked/flushing exits get
 * edge reload blocks; every plain &&/nested-if spelling compiles 4 words short;
 *  - `if (!flushing) { ...; same = 1; goto flush; } continue;` (retail's dead `b next` block);
 *  - other-port loop: `if (i == j) continue;` then `if (inited && !memcmp) { ... move: ... break; }` (the extra
 * continue block is retail's ugen temp pop; with `i != j &&` the t6-t9 ring runs one phase off)
 *  - the directory loop reuses `ret` for osPfsFileState (one variable = one register: retail keeps it in s0 as in
 * the init section) ; `if (file_size) {.}`;
 *  - ClearLL(&gPS_Files[j]) = ClearLL inlined; the first rumble clear is the one-site inlined helper
 *    _StopRumble (retail's +8 inline area above the first lock), the second is inline code;
 *  - `s8 status = 0; s8 retry = 0;` initialisers; `dup = 0; same = 0; memcpy(...)` on one line (as1 line tie);
 * - DISCLOSED DEVICE: ONE compiled-out `if (forceUpdate) {}` at the end (needed three; levers made
 *    two redundant). Without it umerge also inlines retail's first `jal si_lock` (the public pak lock):
 *    retail's caller is one branch larger at the IR level. Retail also stores forceUpdate to its home at entry
 *    (`sw a0,432(sp)`) and never loads it, so retail references the parameter in code that compiles away.
 *  - DISCLOSED UNUSED LOCALS (exact frame residual): `s32 err` (sp+416) and `s8 X` (sp+311) are unreferenced.
 *    Retail frame 432: i 428, j 424, ret 420, [416], pfs 312-415, [311], status 310, retry 309, changed 304-307,
 *    dup 303, same 302, fileh 296, psfile 292 (retail touches j 424, pfs 312/316, 311-302 and 296 exactly as
 *    here). Without err pfs moves to 316 (unit: 41 words differ); without X status/retry/changed/dup/same move up
 *    one byte (12 words differ). The frame stays 432 either way; two slots, 5 bytes, identities unknown.
 *
 * Shaping in PS_DeleteFile (source-level, nothing compiled out): the retry is a goto loop (`delete_retry:`, the
 *  matched _PS_FlushFile's `write_retry:` style); `fileNo` and `state` are locals; `error` is declared after psfile
 *  (retail's psfile 208 / retry 215 / status 214; the last-declared 4-byte local lives in a register).
 *
 * Shaping in the shared free-list helper pfs_get_node (inlined in PS_CreateFile and PS_PakServer):
 *  - DISCLOSED DEVICE: a compiled-out `if (pak_free_handles.head) {}` before `fileh = pak_free_handles.head;`, and fileh (not a
 *    second head load) passed to RemoveLLE. It splits the head load across two uopt blocks, which makes the loaded
 *    head a global expression web: in PS_PakServer it then gets its own spill home (retail's ext/game name pointers
 *    at 96/100, not 100/104) and v0 (retail `lui v0; lw a1,24808(v0)`). PS_CreateFile stays EQUAL with it.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef void *OSMesg;
typedef struct OSThread OSThread;
typedef struct OSMesgQueue {
    OSThread *mtqueue, *fullqueue;
    s32 validCount, first, msgCount;
    OSMesg *msg;
} OSMesgQueue;
typedef struct OSPfs {
    s32 status;
    OSMesgQueue *queue;
    s32 channel;
    u8 id[32], label[32];
    s32 version, dir_size, inode_table, minode_table, dir_table, inode_start_page;
    u8 banks, activebank;
} OSPfs;
typedef struct OSPfsState {
    u32 file_size, game_code;
    u16 company_code;
    u8 ext_name[4], game_name[16];
} OSPfsState;
/* one Controller Pak file slot (40 bytes; offsets from retail) */
typedef struct PakFile {
    s8 flush, flag1, flag2, opaque3;
    u16 mapBytes, opaque6;
    OSPfsState state;
} PakFile;
/* gPaks[pak] (GC anon_changed_inserted_25c; N64 772 bytes) */
typedef struct PakRecord {
    u8 changed;
    s8 inserted, opaque2, opaque3, opaque4, damaged, opaque6, flushing, flushed, opaque9, opaque10, flush;
    OSPfs pfs;
    s32 error, FreeSpace;
    u8 motorOn, request, level, opaque127;   /* rumble thread state (rumble_thread) */
    s16 phase;
    u8 opaque130[2];
    PakFile files[16];
} PakRecord;
typedef struct tPS_File tPS_File;
typedef struct PakHandle { tPS_File *file; } PakHandle;
/* GC tPS_File (N64 76 bytes): ll, RemoveFile, MoveFile, pak, file number, name/ext, size, blocks, data */
struct tPS_File {
    PakHandle *next;
    PakHandle *prev;                  /* PC: a pointer (InsertLLE's link); the record is 96 bytes (pc_prelude.h) */
    void (*RemoveFile)(PakHandle *);
    void (*MoveFile)(PakHandle *, PakHandle *);
    u8 pak, fileNo;
    u8 name[35], ext[11];
    u32 size, blocks;
    u8 **data;
};
typedef struct tDLList {
    u8 indirect, doubly, opaque2[2];
    u32 count;
    PakHandle *head, *tail;
} tDLList;
typedef s32 (*PakPredicate)(s32);
typedef void (*PakErrorFunc)(s32, s32, u32, s32, s8 *, s8 *, PakPredicate);

extern volatile PakRecord gPaks[];   /* gPaks */
extern tDLList gPS_Files[];               /* gPS_Files */
extern tDLList pak_free_handles;                 /* free tPS_File handles */
extern s8 si_lock_inited, gErrPak;           /* lock queue initialised; gErrPak */
extern OSMesgQueue si_lock_queue;
extern OSMesg si_lock_msg;
extern PakErrorFunc gErrorFunc;            /* gErrorFunc */
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
extern s32 osPfsAllocateFile(OSPfs *, u16, u32, u8 *, u8 *, s32, s32 *);
extern s32 osPfsFileState(OSPfs *, s32, OSPfsState *);
extern s32 osPfsFreeBlocks(OSPfs *, s32 *);
extern void pak_encode_name(u8 *, u8 *, u8);          /* ASCII -> pak font */
extern void pak_decode_name(u8 *, u8 *, u8);          /* pak font -> ASCII */
extern s32 game_strcmp(u8 *, u8 *);               /* strcmp */
extern s32 pak_removed(s32);                      /* !gPaks[pak].inserted */
extern void PS_DeleteFile(PakHandle *);     /* PS_DeleteFile */
extern void RemoveLLE(tDLList *, PakHandle *);  /* RemoveLLE */
extern void InsertLLE(tDLList *, PakHandle *, PakHandle *);  /* InsertLLE */
extern void *AllocateHandleHeap(void *, u32);      /* AllocateHandleHeap */
extern void *memset(void *, s32, u32);
void pak_insert_file(PakHandle *fileh);
/* context: byte-identical copy of the locked pfs_status body (internal here: IPA register facts) */
s32 pak_error_from_pfs(s32 error);

/* the inlined pak lock helpers (bodies as group pak_write_cluster pfs_*; own names: the unit renames duplicated internal names) */
void paksys_lock(void);
void paksys_unlock(void);

/* 0x800A2D0C: InsertLLE(&gPS_Files[file->pak], fileh, head) */
void pak_insert_file(PakHandle *fileh)
{
    InsertLLE(&gPS_Files[fileh->file->pak], fileh, gPS_Files[fileh->file->pak].head);
}

/* retail stub 0x800A2D44: take a tPS_File handle off the free list (compiled-out head test: see the header) */
PakHandle *pfs_get_node(void);

/* 0x800A2D4C */
PakHandle *PS_CreateFile(s32 pak, u8 *name, u8 *ext, u32 numBytes, s32 company, s32 gameCode)
{
    s32 error;
    s32 file;
    u8 game_name[16];
    u8 ext_name[4];
    s8 retry;
    s8 status;
    PakHandle *fileh;
    tPS_File *psfile;
    OSPfsState *state;

    paksys_lock();
    pak_encode_name(game_name, name, 16);
    pak_encode_name(ext_name, ext, 4);
    for (;;) {
        error = osPfsAllocateFile(&gPaks[pak].pfs, (u16)company, (u32)gameCode,
                                  game_name, ext_name, (numBytes + 255) & ~255U, &file);
        if (error) {
            gPaks[pak].error = pak_error_from_pfs(error);
            paksys_unlock();
            gErrPak = pak;
            gErrorFunc(pak, gPaks[pak].error, (numBytes + 255) >> 8, 0, &retry, &status,
                       gPaks[pak].error == 3 ? 0 : pak_removed);
            gErrPak = -1;
            if (gPaks[pak].error != 3 && !gPaks[pak].inserted) return 0;
            paksys_lock();
            if (gPaks[pak].error == 8 && retry) {
                /* the file exists: find its slot and delete our handle for it */
                for (file = 0; file < 16; file++) {
                    state = &gPaks[pak].files[file].state;
                    if (game_strcmp(state->game_name, game_name) == 0 &&
                        game_strcmp(state->ext_name, ext_name) == 0 &&
                        state->game_code == (u32)gameCode &&
                        company == state->company_code) break;
                }
                if (file == 16) continue;
                paksys_unlock();
                for (fileh = gPS_Files[pak].head; fileh != 0; fileh = fileh->file->next) {
                    if (fileh->file->fileNo == file) {
                        PS_DeleteFile(fileh);
                        break;
                    }
                }
                paksys_lock();
            }
            if (gPaks[pak].error == 6 && retry) retry = 0;
            if (!retry) {
                paksys_unlock();
                return 0;
            }
        } else break;
    }
    osPfsFreeBlocks(&gPaks[pak].pfs, &gPaks[pak].FreeSpace);
    for (;;) {
        error = osPfsFileState(&gPaks[pak].pfs, file, &gPaks[pak].files[file].state);
        if (!error && gPaks[pak].files[file].state.file_size != 0) break;
        gPaks[pak].error = pak_error_from_pfs(error);
        paksys_unlock();
        gErrPak = pak;
        gErrorFunc(pak, gPaks[pak].error, (numBytes + 255) >> 8, 0,
                   &retry, &status, pak_removed);
        gErrPak = -1;
        if (!gPaks[pak].inserted) {
            gPaks[pak].files[file].state.file_size = 0;
            return 0;
        }
        paksys_lock();
        if (!retry) {
            paksys_unlock();
            gPaks[pak].files[file].state.file_size = 0;
            return 0;
        }
    }
    fileh = pfs_get_node();
    psfile = fileh->file;
    psfile->RemoveFile = 0;
    psfile->MoveFile = 0;
    psfile->pak = pak;
    psfile->fileNo = file;
    psfile->size = gPaks[pak].files[file].state.file_size;
    psfile->blocks = (psfile->size + 255) >> 8;
    gPaks[pak].files[file].mapBytes = (((psfile->size + 31) >> 5) + 7) >> 3;
    psfile->data = AllocateHandleHeap(0, gPaks[pak].files[file].mapBytes + psfile->size);
    memset(*psfile->data, 0, psfile->size);
    memset(*psfile->data + psfile->size, 255, gPaks[pak].files[file].mapBytes);
    gPaks[pak].files[file].flush = 0;
    gPaks[pak].files[file].flag1 = 0;
    gPaks[pak].files[file].flag2 = 0;
    pak_decode_name(psfile->name, gPaks[pak].files[file].state.game_name, 16);
    pak_decode_name(psfile->ext, gPaks[pak].files[file].state.ext_name, 4);
    pak_insert_file(fileh);
    paksys_unlock();
    return fileh;
}



/* ---- PS_DeleteFile (0x800A2680) ---- */
extern s32 osPfsDeleteFile(OSPfs *, u16, u32, u8 *, u8 *);
extern OSMesgQueue heap_lock_queue;          /* heap semaphore */
extern void DeallocateMemoryInternal(u32, s32);  /* DeallocateMemory core (IPA a1/a2) */
void pdf_dealloc(u32 address);
void pdf_free_blocks(s32 pak);
void pdf_release(PakHandle *fileh);

/* 0x800A2680: PS_DeleteFile */
/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void PS_DeleteFile(PakHandle *fileh)
{
    s32 pak;
    s32 fileNo;
    OSPfsState *state;
    PakHandle *cur;
    s8 retry;
    s8 status;
    tPS_File *psfile;
    s32 error;

    paksys_lock();
    psfile = fileh->file;
    pak = psfile->pak;
    fileNo = psfile->fileNo;
    state = (OSPfsState *)&gPaks[pak].files[fileNo].state;
delete_retry:
    error = osPfsDeleteFile((OSPfs *)&gPaks[pak].pfs, state->company_code, state->game_code,
                            state->game_name, state->ext_name);
    if (error) {
        gPaks[pak].error = pak_error_from_pfs(error);
        paksys_unlock();
        gErrPak = pak;
        gErrorFunc(pak, gPaks[pak].error, 0, 0, &retry, &status, pak_removed);
        gErrPak = -1;
        for (cur = gPS_Files[pak].head; cur != 0; cur = cur->file->next) {
            if (fileh == cur) break;
        }
        if (cur == 0) return;
        paksys_lock();
        if (retry) goto delete_retry;
        paksys_unlock();
        return;
    }
    pdf_free_blocks(pak);
    if (psfile->RemoveFile) psfile->RemoveFile(fileh);
    state->file_size = 0;
    if (psfile->data) {
        pdf_dealloc(N64A(psfile->data));
        psfile->data = 0;
    }
    pdf_release(fileh);
    paksys_unlock();
}

/* ---- PS_PakServer and its private callees ---- */
typedef struct ControllerData { s8 present; u8 opaque1[15]; } ControllerData;
extern ControllerData gControllerData[];     /* gControllerData */
extern s8 gPakSysInited;                   /* gInited */
extern OSMesgQueue si_event_queue;          /* SI message queue */
extern OSMesgQueue heap_lock_queue;          /* heap semaphore */
extern void si_lock(void);        /* pak lock (public; the first call stays a jal: umerge budget) */
extern s32 osPfsInitPak(OSMesgQueue *, OSPfs *, s32);
extern s32 osMotorInit(OSMesgQueue *, OSPfs *, s32);
extern s32 osPfsRepairId(OSPfs *);
extern s32 game_memcmp(u8 *, u8 *, s32);   /* memcmp */
extern s32 PS_FlushPak(s32, s32);
extern void *memcpy(void *, const void *, u32);
extern void DeallocateMemoryInternal(u32, s32);  /* DeallocateMemory */
extern void ClearLL(tDLList *);       /* ClearLL */

/* 0x800A3640: _RemovePak (private IPA: unsaved s-registers) */
void RemovePak(s32 pak)
{
    PakHandle *fileh;
    PakHandle *next;
    tPS_File *psfile;

    fileh = gPS_Files[pak].head;
    while (fileh != 0) {
        psfile = fileh->file;
        next = psfile->next;
        if (psfile->RemoveFile != 0) {
            psfile->RemoveFile(fileh);
        }
        if (psfile->data != 0) {
            u8 **data = psfile->data;
            osRecvMesg(&heap_lock_queue, 0, 1);
            DeallocateMemoryInternal(N64A(data), 0);
            osSendMesg(&heap_lock_queue, 0, 0);
            psfile->data = 0;
        }
        RemoveLLE(&gPS_Files[fileh->file->pak], fileh);
        InsertLLE(&pak_free_handles, fileh, pak_free_handles.head);
        fileh = next;
    }
}

/* 0x800A3724: _ClearPak (private IPA: pak in a3) */
void ClearPak(u8 pak)
{
    s8 damaged;

    damaged = gPaks[pak].damaged;
    memset((void *)&gPaks[pak], 0, sizeof(PakRecord));
    gPaks[pak].damaged = damaged;
}

/* inlined: does the file's write bitmap have dirty bytes */
s32 _FileChanged(tPS_File *psfile);

/* inlined: motor off and rumble state cleared */
void _StopRumble(s32 pak);

/* 0x800A377C: PS_PakServer */
void PS_PakServer(s32 forceUpdate, s32 rumbleOnly)
{
    s32 i;
    s32 j;
    s32 ret;
    s32 err;                /* unreferenced: frame slot sp+416 (disclosed) */
    OSPfs pfs;
    s8 X;                   /* unreferenced: frame slot sp+311 (disclosed) */
    s8 status = 0;
    s8 retry = 0;
    s8 changed[4];
    s8 dup;
    s8 same;
    PakHandle *fileh;
    tPS_File *psfile;

    if (!gPakSysInited) return;
    for (i = 0; i < 4; i++) changed[i] = 0;
    si_lock();
    for (i = 0; i < 4; i++) {
        if (gErrPak >= 0 && i != gErrPak) continue;
        if (!gControllerData[i].present || !gPaks[i].opaque2 || gPaks[i].opaque3) {
            gPaks[i].opaque3 = 0;
            if (gPaks[i].inserted) {
                gPaks[i].inserted = 0;
                gPaks[i].opaque9 = 0;
                gPaks[i].error = 0;
                if (gPaks[i].opaque6) {
                    _StopRumble(i);
                    changed[i] = 1;
                    continue;
                }
                if (gPaks[i].opaque10) goto rj;
                if (!gPaks[i].flush) {
                    if (gPaks[i].flushing) goto rj;
                    RemovePak(i);
                    ClearPak(i);
                }
rj:
                gPaks[i].damaged = 0;
                changed[i] = 1;
            } else if (gPaks[i].opaque4 && !gPaks[i].opaque10 && !gPaks[i].flush && !gPaks[i].flushing) {
                RemovePak(i);
                ClearPak(i);
            }
            continue;
        }
        if (gPaks[i].inserted) continue;
        changed[i] = 1;
        gPaks[i].inserted = 1;
        dup = 0; same = 0; memcpy(&pfs, (void *)&gPaks[i].pfs, sizeof(OSPfs));
init:
        pfs.queue = 0;
        ret = osPfsInitPak(&si_event_queue, &pfs, i);
        if (!pfs.queue) ret = 1;
        gPaks[i].error = pak_error_from_pfs(ret);
        if (gPaks[i].error == 10) {
            if (!osMotorInit(&si_event_queue, &pfs, i)) {
                if (!gPaks[i].opaque6) gPaks[i].opaque6 = 1;
                else changed[i] = 0;
                continue;
            }
            if (rumbleOnly) {
                gPaks[i].inserted = 0;
                changed[i] = 0;
                continue;
            }
            paksys_unlock();
prompt:
            gErrPak = i;
            gErrorFunc(i, 10, 0, 0, &retry, &status, pak_removed);
            gErrPak = -1;
            if (!gPaks[i].inserted) {
                si_lock();
                changed[i] = 0;
                continue;
            }
            if (status) {
                si_lock();
                ret = osPfsInitPak(&si_event_queue, &pfs, i);
                if (!ret) goto inited;
                ret = osPfsRepairId(&pfs);
                paksys_unlock();
                if (ret) {
                    gErrPak = i;
                    gErrorFunc(i, 12, 0, 0, &retry, &status, 0);
                    gErrPak = -1;
                    goto prompt;
                }
                gErrPak = i;
                gErrorFunc(i, 13, 0, 0, &retry, &status, 0);
                gErrPak = -1;
            } else if (retry) {
                si_lock();
                gPaks[i].damaged = 1;
                gPaks[i].opaque9 = 0;
                changed[i] = 0;
                continue;
            } else {
                gErrPak = i;
                gErrorFunc(i, 11, 0, 0, &retry, &status, 0);
                gErrPak = -1;
            }
            si_lock();
            goto init;
        }
inited:
        gPaks[i].opaque6 = 0;
        gPaks[i].phase = 0;
        gPaks[i].level = 0;
        gPaks[i].request = 0;
        gPaks[i].motorOn = 0;
        if (ret == 2) gPaks[i].error = 0;
        else if (ret) {
            changed[i] = 0;
            gPaks[i].damaged = 1;
            gPaks[i].opaque9 = 0;
            continue;
        }
        if (!gPaks[i].opaque4) {
            gPaks[i].opaque4 = 1;
        } else if (!game_memcmp((u8 *)gPaks[i].pfs.id, pfs.id, 32)) {
            changed[i] = 0;
            memcpy((void *)&gPaks[i].pfs, &pfs, sizeof(OSPfs));
            osPfsFreeBlocks((OSPfs *)&gPaks[i].pfs, (s32 *)&gPaks[i].FreeSpace);
            gPaks[i].opaque9 = 1;
            if (!gPaks[i].flushing) {
                for (fileh = gPS_Files[i].head; fileh; fileh = fileh->file->next) {
                    if (_FileChanged(fileh->file)) {
                        gPaks[i].files[fileh->file->fileNo].flag1 = 1;
                        gPaks[i].flush = 1;
                    }
                }
                same = 1;
                goto flush;
            }
            continue;
        }
        for (j = 0; j < 4; j++) {
            if (i == j) continue;
            if (gPaks[j].opaque4 && !game_memcmp(pfs.id, (u8 *)gPaks[j].pfs.id, 32)) {
                if (gPaks[i].opaque4) {
                    dup = 1;
                    goto locked;
                }
move:
                memcpy((void *)&gPaks[i].pfs, &pfs, sizeof(OSPfs));
                osPfsFreeBlocks((OSPfs *)&gPaks[i].pfs, (s32 *)&gPaks[i].FreeSpace);
                gPaks[i].opaque9 = 1;
                memcpy(&gPS_Files[i], &gPS_Files[j], sizeof(tDLList));
                ClearLL(&gPS_Files[j]);
                for (fileh = gPS_Files[i].head; fileh; fileh = fileh->file->next)
                    fileh->file->pak = i;
                memcpy((void *)gPaks[i].files, (void *)gPaks[j].files, sizeof(gPaks[i].files));
                changed[i] = 0;
                gPaks[j].opaque4 = 0;
                gPaks[j].opaque9 = 0;
                if (gPaks[j].inserted && !gPaks[j].damaged) gPaks[j].opaque3 = 1;
                changed[j] = 1;
                break;
            }
        }
        if (j < 4) continue;
        if (gPaks[i].flushing) {
            changed[i] = 0;
            continue;
        }
locked:
        if (gPaks[i].opaque10) {
            changed[i] = 0;
            continue;
        }
flush:
        paksys_unlock();
        if (!PS_FlushPak(i, 0)) {
            changed[i] = 0;
            si_lock();
            continue;
        }
        si_lock();
        if (gPaks[i].flushed && !same) {
            dup = 0;
            same = 0;
            goto init;
        }
        if (same) continue;
        RemovePak(i);
        if (dup) goto move;
        gPaks[i].opaque9 = 1;
        gPaks[i].opaque10 = 0;
        gPaks[i].flush = 0;
        memcpy((void *)&gPaks[i].pfs, &pfs, sizeof(OSPfs));
        osPfsFreeBlocks((OSPfs *)&gPaks[i].pfs, (s32 *)&gPaks[i].FreeSpace);
        for (j = 0; j < 16; j++) {
            ret = osPfsFileState((OSPfs *)&gPaks[i].pfs, j, (OSPfsState *)&gPaks[i].files[j].state);
            if (ret) gPaks[i].files[j].state.file_size = 0;
            gPaks[i].files[j].flush = 0;
            gPaks[i].files[j].flag1 = 0;
            gPaks[i].files[j].flag2 = 0;
            gPaks[i].files[j].mapBytes = (((gPaks[i].files[j].state.file_size + 31) >> 5) + 7) >> 3;
            gPaks[i].error = pak_error_from_pfs(ret);
            if (gPaks[i].files[j].state.file_size) {
                fileh = pfs_get_node();
                psfile = fileh->file;
                psfile->RemoveFile = 0;
                psfile->MoveFile = 0;
                psfile->pak = i;
                psfile->fileNo = j;
                psfile->size = gPaks[i].files[j].state.file_size;
                psfile->blocks = (psfile->size + 255) >> 8;
                pak_decode_name(psfile->name, (u8 *)gPaks[i].files[j].state.game_name, 16);
                pak_decode_name(psfile->ext, (u8 *)gPaks[i].files[j].state.ext_name, 4);
                if (psfile->ext[0]) psfile->ext[1] = 0;
                psfile->data = 0;
                pak_insert_file(fileh);
            }
        }
    }
    paksys_unlock();
    for (i = 0; i < 4; i++) gPaks[i].changed = changed[i];
    if (forceUpdate) {}     /* disclosed umerge-budget device (one branch) */
}

/* ---- PS_InitPakSys ---- */
extern PakHandle pak_file_handles[];          /* the 64 tPS_File handles */
extern tPS_File pak_file_records[];           /* the 64 tPS_File records (76 bytes) */
extern u8 rumble_thread[];                 /* pak thread */
extern u8 rumble_thread_stack_top[];                 /* pak thread stack top */
extern void rumble_thread_entry(void *);    /* rumble/pak thread entry */
extern void osCreateThread(void *, s32, void (*)(void *), void *, void *, s32);
extern void osStartThread(void *);
extern void pak_poll_inserted(void);        /* poll pak presence (_CompleteCallback slot) */
extern void InitLL(tDLList *, u8, u8);   /* InitLL (MAT3 char = unsigned) */

/* 0x800A4508: PS_InitPakSys */
void PS_InitPakSys(PakErrorFunc errorFunc)
{
    s32 i;

    gErrorFunc = errorFunc;
    InitLL(&pak_free_handles, 1, 1);
    for (i = 0; i < 64; i++) {
        pak_file_handles[i].file = &pak_file_records[i];
        InsertLLE(&pak_free_handles, &pak_file_handles[i], pak_free_handles.head);
    }
    for (i = 0; i < 4; i++) {
        InitLL(&gPS_Files[i], 1, 1);
        gPaks[i].damaged = 0;
        ClearPak(i);
    }
    osCreateThread(rumble_thread, 2, rumble_thread_entry, 0, rumble_thread_stack_top, 6);
    osStartThread(rumble_thread);
    gPakSysInited = 1;
    pak_poll_inserted();
}
