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
void paksys_queue_init(void) {
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue, &si_lock_msg, 1);
        osSendMesg(&si_lock_queue, 0, 0);
    }
}
void paksys_lock(void) {
    OSMesg message;
    paksys_queue_init();
    osRecvMesg(&si_lock_queue, &message, 1);
}
void paksys_unlock(void) {
    osSendMesg(&si_lock_queue, 0, 0);
}

/* 0x800A2D0C: InsertLLE(&gPS_Files[file->pak], fileh, head) */
void pak_insert_file(PakHandle *fileh);

/* retail stub 0x800A2D44: take a tPS_File handle off the free list (compiled-out head test: see the header) */
PakHandle *pfs_get_node(void)
{
    PakHandle *fileh;
    if (pak_free_handles.head) {}
    fileh = pak_free_handles.head;
    RemoveLLE(&pak_free_handles, fileh);
    return fileh;
}

/* 0x800A2D4C */



/* ---- PS_DeleteFile (0x800A2680) ---- */
extern s32 osPfsDeleteFile(OSPfs *, u16, u32, u8 *, u8 *);
extern OSMesgQueue heap_lock_queue;          /* heap semaphore */
extern void DeallocateMemoryInternal(u32, s32);  /* DeallocateMemory core (IPA a1/a2) */
void pdf_heap_lock(void)
{
    osRecvMesg(&heap_lock_queue, 0, 1);
}
void pdf_heap_unlock(void)
{
    osSendMesg(&heap_lock_queue, 0, 0);
}
void pdf_dealloc(u32 address)
{
    pdf_heap_lock();
    DeallocateMemoryInternal(address, 0);
    pdf_heap_unlock();
}
void pdf_free_blocks(s32 pak)
{
    osPfsFreeBlocks((OSPfs *)&gPaks[pak].pfs, (s32 *)&gPaks[pak].FreeSpace);
}
void pdf_release(PakHandle *fileh)
{
    RemoveLLE(&gPS_Files[fileh->file->pak], fileh);
    InsertLLE(&pak_free_handles, fileh, pak_free_handles.head);
}

/* 0x800A2680: PS_DeleteFile */
void PS_DeleteFile(PakHandle *fileh);


/* ---- PS_ReadPartialFile (0x800A2990) ---- */
extern s32 osPfsReadWriteFile(OSPfs *, s32, u8, s32, s32, u8 *);
extern void osInvalDCache(void *, s32);

void prpf_nop(void)
{
}

/* 0x800A2990: PS_ReadPartialFile(fileh, offset, numBytes) */
/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void PS_ReadPartialFile(PakHandle *fileh, s32 offset, u32 numBytes)
{
    tPS_File *psfile;
    s32 pak;
    PakHandle *cur;
    s32 error;
    s32 fileNo;
    s8 retry;
    s8 status;
    u32 size;

    psfile = fileh->file;
    pak = psfile->pak;
    fileNo = psfile->fileNo & 0xFF;
    if (psfile->data) return;
    if (offset > psfile->size) {}
    if (offset) {}
    psfile->data = AllocateHandleHeap(0, gPaks[pak].files[fileNo].mapBytes + psfile->size);
    memset(*psfile->data + psfile->size, 0, gPaks[pak].files[fileNo].mapBytes);
    osInvalDCache(*psfile->data, psfile->size);
    prpf_nop();
    paksys_lock();
    size = numBytes;
    do {} while (0);
read_retry:
    error = osPfsReadWriteFile((OSPfs *)&gPaks[pak].pfs, fileNo, 0, offset,
                               ((size + 31) >> 5) << 5, *psfile->data);
    if (error) {
        gPaks[pak].error = pak_error_from_pfs(error);
        if (gPaks[pak].error == 9) {
            paksys_unlock();
            PS_DeleteFile(fileh);
            return;
        }
        paksys_unlock();
        gErrPak = pak;
        gErrorFunc(pak, gPaks[pak].error, 0, 0, &retry, &status, pak_removed);
        gErrPak = -1;
        paksys_lock();
        if (retry) goto read_retry;
        for (cur = gPS_Files[pak].head; cur != 0; cur = cur->file->next) {
            if (fileh == cur) {
                if (psfile->data) {
                    pdf_dealloc(N64A(psfile->data));
                    psfile->data = 0;
                }
                break;
            }
        }
        paksys_unlock();
        return;
    }
    paksys_unlock();
    gPaks[pak].files[fileNo].flag2 = numBytes != psfile->size;
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

/* 0x800A3724: _ClearPak (private IPA: pak in a3) */

/* inlined: does the file's write bitmap have dirty bytes */
s32 _FileChanged(tPS_File *psfile)
{
    u8 *map;
    u32 i;

    if (!psfile->data) return 0;
    map = *psfile->data + psfile->size;
    for (i = 0; i < gPaks[psfile->pak].files[psfile->fileNo].mapBytes; i++) {
        if (map[i]) return 1;
    }
    return 0;
}

/* inlined: motor off and rumble state cleared */
void _StopRumble(s32 pak)
{
    gPaks[pak].opaque6 = 0;
    gPaks[pak].phase = 0;
    gPaks[pak].level = 0;
    gPaks[pak].request = 0;
    gPaks[pak].motorOn = 0;
}

/* 0x800A377C: PS_PakServer */

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
