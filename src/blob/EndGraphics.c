/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * EndGraphics (0x800EDACC, 540 B) = N64 EndGraphics (gfx.c; DC gfx.c func_8C02E390 is the Dreamcast
 * counterpart; MAT3 ngcGraph.c EndGraphics is the GameCube stub). Called by the main-thread message pump
 * (process_scheduler_message) on message 2750: HAL_FinishText(1) (HAL_FinishText); closes the frame's display
 * list (text DL from BlitFinish, full sync, end); fills the scheduler task of frame record frame_records[frame_record_index]
 * (data_ptr/data_size = the Dynamic buffer's glist and its used size, rspboot, gfx ucode, dram stack, output buffer,
 * yield buffer, flags = pending (gfx_tasks_pending) ? 2 : 0, done-message queue); bumps gRenderInProgress around
 * osStopThread/osStartThread of the scheduler thread, posts the task (osSendMesg) and copies the scheduler's frame count
 * (gScheduler+636) to gLastFrameCount.
 * frame_dynamic is the frame's command buffer (SDK "Dynamic"): glist[4000] at +0x9CC0 (the two display buffers
 * frame_command_buffer_a / frame_command_buffer_b are 0x119C0 bytes apart = 0x9CC0 + 4000 * sizeof(Gfx)); frame_matrix_cursor = +1728.
 * (from 88-row draft wcr/best/gfx8_volatile.c). DISCLOSED SHAPING: two compiled-out display-list
 * overflow checks (`if (...) {}`, no code; the release build's error print is gone):
 *   1. `glistp > &dynamicp->glist[GLIST_LEN]` after the end-of-list command: its frame_dynamic load is shared with
 *      data_ptr (retail's t0 web) and splits the block, so &frame_dynamic takes t1 and rspboot v0 as in retail;
 *   2. `glistp - dynamicp->glist > GLIST_LEN` after the flags test: the second use of the constant -0x9CC0 (retail
 *      keeps it in v1) and the extra blocks that put the cursor's address web in a3 after the three GBI `_g` webs.
 * Any compare constant/operator works (measured: 100, 3999, 4000, 4096, >=); 4000 is the buffer's real length.
 * Statement order after the flags test (ucode, ucode_data, ..., framebuffer last) is the ugen temp-ring order retail
 * shows (ucode_data and the f->buffer load share t2). Volatile scheduler globals as in mainloop. No unused
 * locals (frame 32 = 16 args + f, dl + s0/ra).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef void *OSMesg;
typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef struct { u32 w0; uintptr_t w1; } Gwords;
typedef union { Gwords words; long long force_structure_alignment; } Gfx;
#define gDma1p_(pkt, c, s) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = (c); _g->words.w1 = (uintptr_t)(s); }
#define gSPDisplayList(pkt, dl) gDma1p_(pkt, 0xDE000000, dl)
#define gDPFullSync(pkt) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = 0xE9000000; _g->words.w1 = 0; }
#define gSPEndDisplayList(pkt) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = 0xDF000000; _g->words.w1 = 0; }
typedef struct {
    u32 type;               /* 0x10 */
    u32 flags;              /* 0x14 */
    u64 *ucode_boot;        /* 0x18 */
    u32 ucode_boot_size;    /* 0x1C */
    u64 *ucode;             /* 0x20 */
    u32 ucode_size;         /* 0x24 */
    u64 *ucode_data;        /* 0x28 */
    u32 ucode_data_size;    /* 0x2C */
    u64 *dram_stack;        /* 0x30 */
    u32 dram_stack_size;    /* 0x34 */
    u64 *output_buff;       /* 0x38 */
    u64 *output_buff_size;  /* 0x3C */
    u64 *data_ptr;          /* 0x40 */
    u32 data_size;          /* 0x44 */
    u64 *yield_data_ptr;    /* 0x48 */
    u32 yield_data_size;    /* 0x4C */
} OSTask_t;
typedef struct OSScTask {
    struct OSScTask *next;  /* 0x00 */
    u32 state;              /* 0x04 */
    u32 flags;              /* 0x08 */
    void *framebuffer;      /* 0x0C */
    OSTask_t t;             /* 0x10 */
    OSMesgQueue *msgQ;      /* 0x50 */
    OSMesg msg;             /* 0x54 */
    void *pad58;            /* the DL base pointer (other views) */
    u8 doneMsg[0x7C - 0x5C];/* 0x5C */
    void *buffer;           /* 0x7C */
} Frame;                    /* 0x80 */
typedef char pc_frame_size_check[(sizeof(Frame) == 0xD0 && __builtin_offsetof(Frame, pad58) == 0xA0) ? 1 : -1];

extern Gfx *frame_dl;             /* display-list cursor (frame_dl) */
extern volatile s8 frame_record_index;               /* current frame record */
extern Frame frame_records[];
extern Gfx *last_frame_dl_end;
extern s32 last_frame_text_dl_end;
extern intptr_t blit_dl;
/* PC: the frame's buffer as pc_game_types.h lays it out, PC_GLIST_LEN commands from PC_GLIST_OFFSET; at the end of
 * its list pc_frame_pools checks the list, the frame's matrices (frame_matrix_cursor) and its lookats, which the SDK's empty
 * `if` didn't: a Debug build stops when one ran past its end (docs/port/MORECARS.md 8). */
#define GLIST_LEN PC_GLIST_LEN
#define PC_FRAME_OVER pc_frame_pools(frame_dynamic, frame_dl, frame_matrix_cursor)
extern u8 *frame_matrix_cursor;
typedef struct {
    u8 pad[PC_GLIST_OFFSET];
    Gfx glist[GLIST_LEN];
} Dynamic;
extern Dynamic *frame_dynamic;
extern s32 gfx_tasks_pending;
extern u64 rspbootTextStart[], rspbootTextEnd[];   /* rspboot */
extern u64 gspF3DEX2_NoN_fifoTextStart[];                 /* gfx ucode text */
extern u64 gspF3DEX2_NoN_fifoDataStart[];                 /* gfx ucode data */
extern u64 rsp_dram_stack[], rsp_yield_buffer[], rdp_fifo_buffer[], rdp_fifo_buffer_end[];
extern OSMesgQueue main_thread_queue;
extern OSMesgQueue scheduler_task_queue;
extern u8 scheduler_thread[];                  /* thread */
extern s16 gRenderInProgress;
extern volatile s32 gLastFrameCount;
typedef struct { u8 pad[636]; s32 frameCount; } Sched;
extern volatile Sched gScheduler;
/* PC, game.frame_rate 60 (docs/port/FRAMERATE.md 7): the frame's task waits in the scheduler's task queue until a
 * message on its command queue wakes it (__scMain, 670: schedule). In real time on the PC the RSP finishes the
 * retrace's audio task at once, before the frame is done, so the next message was the next retrace: the frame's
 * task ran a retrace late and was shown two after the frame began, every frame. At 60 the frame's task wakes the
 * scheduler itself, as the audio task's sender does (musyx_submit_audio_task). Original keeps the N64's messages. */
extern OSMesgQueue scheduler_interrupt_queue;
void HAL_FinishText(s32);
Gfx *BlitFinish(void);
void osStopThread(void *);
void osStartThread(void *);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

void EndGraphics(void)
{
    Frame *f;
    Gfx *dl;

    f = &frame_records[frame_record_index];
    last_frame_dl_end = frame_dl;
    last_frame_text_dl_end = blit_dl;
    HAL_FinishText(1);
    dl = BlitFinish();
    gSPDisplayList(frame_dl++, dl);
    gDPFullSync(frame_dl++);
    gSPEndDisplayList(frame_dl++);
    if (PC_FRAME_OVER) {}
    f->t.data_ptr = (u64 *)frame_dynamic->glist;
    f->t.data_size = (u32)((frame_dl - frame_dynamic->glist) * sizeof(Gfx));
    f->t.type = 1;
    f->t.ucode_boot = rspbootTextStart;
    f->t.ucode_boot_size = (u8 *)rspbootTextEnd - (u8 *)rspbootTextStart;
    f->t.ucode_data_size = 2048;
    f->t.dram_stack = rsp_dram_stack;
    f->t.dram_stack_size = 1024;
    if (gfx_tasks_pending == 0) {
        f->t.flags = 0;
    } else {
        f->t.flags = 2;
    }
    if (frame_dl - frame_dynamic->glist > GLIST_LEN) {}
    f->t.ucode = gspF3DEX2_NoN_fifoTextStart;
    f->t.ucode_data = gspF3DEX2_NoN_fifoDataStart;
    f->t.output_buff = rdp_fifo_buffer;
    f->t.output_buff_size = rdp_fifo_buffer_end;
    f->t.yield_data_ptr = rsp_yield_buffer;
    f->t.yield_data_size = 3072;
    f->next = 0;
    f->flags = 99;
    f->msgQ = &main_thread_queue;
    f->msg = &f->doneMsg;
    f->framebuffer = f->buffer;
    osStopThread(scheduler_thread);
    gRenderInProgress++;
    osStartThread(scheduler_thread);
    osSendMesg(&scheduler_task_queue, f, 1); if (pc_fps60) osSendMesg(&scheduler_interrupt_queue, (OSMesg)670, 0);
    gLastFrameCount = gScheduler.frameCount;
}
