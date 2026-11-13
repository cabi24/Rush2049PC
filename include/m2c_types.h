/* include/m2c_types.h: libultra's internal records (the VI context, the PIF RAM block, the scheduler, the timer
 * list, the PI manager) and the boot code's data and functions that the boot segment's C (src/rom, through
 * rom_auto.h and rom_tu.h) shares. Types and declarations only. */
#ifndef M2C_TYPES_H
#define M2C_TYPES_H

/* --- The VI (video interface) context, libultra's __OSViContext: osViInit builds its two records and points
 * __osViContext at the current one (state, retraceCount, framep, modep, control: the SDK's layout). */
typedef struct {
    u32 ctrl, width, burst, vSync, hSync, leap, hStart, xScale, vCurrent;
} OSViCommonRegs;

typedef struct {
    u32 origin, yScale, vStart, vBurst, vIntr;
} OSViFieldRegs;

typedef struct {
    u8 type;
    OSViCommonRegs comRegs;
    OSViFieldRegs fldRegs[2];
} OSViMode;

typedef struct {
    f32 factor;
    u16 offset;
    u32 scale;
} __OSViScale;

typedef struct {
    u16 state;
    u16 retraceCount;
    void *framep;
    OSViMode *modep;
    u32 control;
    OSMesgQueue *msgq;
    OSMesg msg;
    __OSViScale x;
    __OSViScale y;
} __OSViContext;

extern __OSViContext *__osViContext;

/* --- Thread scheduling internals (libultra osint.h), with the SDK's signatures (an earlier naming pass called
 * these two __osCleanupThread and dll_remove). */
extern void __osEnqueueAndYield(OSThread **queue);
void __osDequeueThread(OSThread **queue, OSThread *thread);

/* --- Serial interface raw DMA (libultra os_internal_si.h). ------------- */
extern s32 __osSiRawStartDma(s32 direction, void *dramAddr);

/* --- SI/PFS staging buffers: raw PIF-RAM-sized scratch buffers that
 * controller code builds request/read blocks into before/after DMA.
 * Real libultra OSPifRam is a 64-byte, 0x10-aligned block (15 words +
 * pifstatus); __osPfsPifRam is used the same way by the PFS read path. */
typedef struct {
    u32 ramarray[15];
    u32 pifstatus;
} OSPifRam;

extern OSPifRam __osSiDmaBuffer;
extern u8 __osPfsPifRam[64];

/* __osContRamWrite: standard libultra Controller Pak raw write
 * (PRinternal/controller.h: s32 __osContRamWrite(OSMesgQueue*, int,
 * u16, u8*, int)). */
extern s32 __osContRamWrite(OSMesgQueue *mq, s32 channel, u16 address,
                             u8 *buffer, s32 force);

/* --- Decompression DMA sync helpers (src/util/dma.c, not visible to m2c's
 * type context since that's a .c file, not a header). */
extern s32 acquire_decompress_lock(s32 blocking);
extern void release_decompress_lock(void);

/* --- SI DMA retry counter: the last word of the __osSiDmaBuffer/OSPifRam
 * block (__osSiDmaBuffer + 0x3C, its pifstatus field), which the symbol file
 * gives a symbol of its own. */
extern s32 __osSiDmaRetry;

/* --- Controller Pak raw read (PRinternal/controller.h counterpart to
 * __osContRamWrite above). */
extern s32 __osContRamRead(OSMesgQueue *mq, s32 channel, u16 address,
                            u8 *buffer);

/* --- Scheduler internals (src/libultra/os_scheduler.c has the same
 * layout reconstructed as SCHED_ and TASK_ offset macros operating on
 * void pointers; m2c needs real field types since it decompiles
 * __scAppendList/__scExec's own bodies directly from ROM asm, not through
 * those macros). */
/* PC: the task as the game builds it (EndGraphics.c's Frame, musyx_submit_audio_task's static task): its
 * OSTask is in place from type on, and the OSTask's pointers are host-sized, which moves output_buff,
 * output_buff_size and msgQueue (docs/port/MUSYX_DATA.md 8.4). */
typedef struct OSScTask_s {
    struct OSScTask_s *next;   /* 0x00 */
    s32 state;                  /* 0x04 */
    s32 flags;                  /* 0x08 */
    void *framebuffer;          /* 0x0C */
    s32 type;                   /* 0x10: t.type */
    u32 tflags14;               /* t.flags */
    u64 *pad18; u32 pad1C;      /* t.ucode_boot, size */
    u64 *pad20; u32 pad24;      /* t.ucode, size */
    u64 *pad28; u32 pad2C;      /* t.ucode_data, size */
    u64 *pad30; u32 pad34;      /* t.dram_stack, size */
    void *output_buff;           /* 0x38: t.output_buff */
    s32 *output_buff_size;       /* 0x3C: t.output_buff_size */
    u64 *pad40; u32 pad44;      /* t.data_ptr, data_size */
    u64 *pad48; u32 pad4C;      /* t.yield_data_ptr, size */
    OSMesgQueue *msgQueue;        /* 0x50 */
    OSMesg msg;                    /* 0x54 */
} OSScTask;

typedef struct OSScClient_s {
    struct OSScClient_s *next;  /* 0x00 */
    OSMesgQueue *msgQueue;       /* 0x04 */
} OSScClient;

typedef struct {
    s16 state;                        /* 0x00 */
    u8 pad02[0x20 - 0x02];
    s16 priority;                     /* 0x20 */
    u8 pad22[0x40 - 0x22];
    OSMesgQueue cmdQueue;              /* 0x40 */
    OSMesg cmdMsgs[8];                 /* 0x58 */
    OSMesgQueue retQueue;              /* 0x78 */
    OSMesg retMsgs[8];                 /* 0x90 */
    u8 padB0[0x260 - 0xB0];            /* thread lives here */
    OSScClient *clientList;            /* 0x260 */
    OSScTask *rspTaskHead;             /* 0x264 */
    OSScTask *rspTaskTail;             /* 0x268 */
    OSScTask *rdpTaskHead;             /* 0x26C */
    OSScTask *rdpTaskTail;             /* 0x270 */
    OSScTask *curRSPTask;              /* 0x274 */
    OSScTask *curRDPTask;              /* 0x278 */
    s32 retraceCount;                  /* 0x27C */
    s32 audioListPending;              /* 0x280 */
} OSSched;

extern void __scAppendList(OSSched *sc, OSScTask *task);
extern void __scExec(OSSched *sc, OSScTask *rspTask, OSScTask *rdpTask);

/* osViSwapBuffer/video_apply_pending_mode (symbol_addrs.us.txt names for these two
 * addresses); osViSwapBuffer takes the next framebuffer pointer. */
extern void osViSwapBuffer(void *mode);
extern void video_apply_pending_mode(void);

/* --- Inflate double-buffers + full PI DMA request (libultra osPiStartDma:
 * mb, priority, direction, devAddr, dramAddr, size, mq). */
extern u8 unzip_input_a[0x1000];
extern u8 unzip_input_b[0x1000];
extern s32 osPiStartDma(OSIoMesg *mb, s32 priority, s32 direction,
                              u32 devAddr, void *dramAddr, u32 size,
                              OSMesgQueue *mq);

/* --- PI manager (symbol_addrs.us.txt: osPiInit/__osPiGetAccess/
 * __osPiRelAccess/__osPiMgrState, standard libultra signatures). */
typedef struct {
    s32 flag;   /* 0x00 */
    u8 pad04[0x8 - 0x4];
    s32 unk8;   /* 0x08 */
    u8 pad0C[0x1C - 0xC];
} __OSPiMgrState;

extern __OSPiMgrState __osPiMgrState;
extern void __osPiCreateAccessQueue(void);
extern void __osPiGetAccess(void);
extern void __osPiRelAccess(void);
extern u32 osRomBase;

/* --- RCP/timer/scheduler internals confirmed against ultralib
 * (os_internal_reg.h, os_internal_rsp.h). */
extern void __osSetCompare(u32 compare);
extern void __osSpSetStatus(u32 status);
extern void osSpTaskYield(void);

/* --- __setfpcsr/GameLoop: game_thread_entry sets the FPU control/status
 * register once, then calls the game's state machine, GameLoop, forever. */
extern void __setfpcsr(u32 value);
extern void GameLoop(void);

/* --- SI mutex + PIF pack-read (symbol_addrs.us.txt: __osSiGetAccess/
 * __osSiRelAccess/__osPackReadData are all standard libultra no-arg SI
 * helpers). */
extern void __osSiGetAccess(void);
extern void __osSiRelAccess(void);
extern void __osPackReadData(void);

/* --- Timer queue head (symbol_addrs.us.txt: "timer queue head pointer
 * (TimerQueueHead*)"); dll_init sets up this node as its
 * own circular-list sentinel, touching offsets 0x0/0x4/0x10/0x14/0x18/0x1C
 * -- the same 0x20-byte TimerNode layout documented in src/os/dll.c
 * (next/prev/reload_hi/reload_lo/delta_hi/delta_lo/msgQueue/msg). */
typedef struct __OSTimerNode_s {
    struct __OSTimerNode_s *next;  /* 0x00 */
    struct __OSTimerNode_s *prev;  /* 0x04 */
    s32 reload_hi;                  /* 0x08 */
    s32 reload_lo;                  /* 0x0C */
    s32 delta_hi;                   /* 0x10 */
    s32 delta_lo;                   /* 0x14 */
    OSMesgQueue *msgQueue;           /* 0x18 */
    OSMesg msg;                       /* 0x1C */
} __OSTimerNode;

extern OSTimer *__osTimerList; /* canonical timer layout; overlay above remains available */

/* --- idle_thread_entry calls this with (NULL, 0): libultra osSetThreadPri
 * (a NULL thread means __osRunningThread). */
extern void osSetThreadPri(OSThread *thread, OSPri priority);

/* --- Already-implemented functions (src/util/dma.c, src/libultra/
 * os_thread.c, src/libultra/os_vi.c) not visible to m2c's header-only
 * type context; declare their real signatures so callers type-check. */
extern void init_decompress_lock(void);
extern void osViBlack(s32 mask);
extern void osViSetSpecialFeatures(u32 features);

/* --- Game thread stack (symbol_addrs.us.txt: "game thread stack base"),
 * used only for address arithmetic (base + offset) when setting up the
 * thread's stack pointer. */
extern u8 main_thread_stack[0x2000];

#endif
