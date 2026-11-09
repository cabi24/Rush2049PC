/* EndGraphics: end a frame's display list and hand it to the scheduler (MAT3 gfx.c; the N64 side of it). */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { u32 opaque[6]; } OSMesgQueue;

typedef union { Gwords words; long long force_structure_alignment; } Gfx;
#define gDma1p_(pkt, c, s) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = (c); _g->words.w1 = (uintptr_t)(s); }
#define gSPDisplayList(pkt, dl) gDma1p_(pkt, 0xDE000000, dl)
#define gDPFullSync(pkt) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = 0xE9000000; _g->words.w1 = 0; }
#define gSPEndDisplayList(pkt) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = 0xDF000000; _g->words.w1 = 0; }

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

VIEW(Gfx *, frame_dl);
#define frame_dl frame_dl_view

VIEW(Frame[], frame_records);
#define frame_records frame_records_view
VIEW(Gfx *, last_frame_dl_end);
#define last_frame_dl_end last_frame_dl_end_view

/* PC: the frame's buffer as pc_game_types.h lays it out, PC_GLIST_LEN commands from PC_GLIST_OFFSET; at the end of
 * its list pc_frame_pools checks the list, the frame's matrices (frame_matrix_cursor) and its lookats, which the SDK's empty
 * `if` didn't: a Debug build stops when one ran past its end (docs/port/MORECARS.md 8). */
#define GLIST_LEN PC_GLIST_LEN
#define PC_FRAME_OVER pc_frame_pools(frame_dynamic, frame_dl, frame_matrix_cursor)

typedef struct Dynamic {
    u8 pad[PC_GLIST_OFFSET];
    Gfx glist[GLIST_LEN];
} Dynamic;

extern OSMesgQueue scheduler_task_queue;
extern u8 scheduler_thread[];                  /* thread */

/* PC, game.frame_rate 60 (docs/port/FRAMERATE.md 7): the frame's task waits in the scheduler's task queue until a
 * message on its command queue wakes it (__scMain, 670: schedule). In real time on the PC the RSP finishes the
 * retrace's audio task at once, before the frame is done, so the next message was the next retrace: the frame's
 * task ran a retrace late and was shown two after the frame began, every frame. At 60 the frame's task wakes the
 * scheduler itself, as the audio task's sender does (musyx_submit_audio_task). Original keeps the N64's messages. */
extern OSMesgQueue scheduler_interrupt_queue;

void osStopThread(void *);
void osStartThread(void *);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

#define M_GFXTASK 1                 /* OSTask type: a graphics task */
#define OS_TASK_DP_WAIT 2           /* OSTask flag: wait for the RDP to finish the last task's output */
#define OS_SC_SWAP_AND_LAST 99      /* the frame record's scheduler flags (OS_SC_NEEDS_RSP | RDP | LAST_TASK | SWAPBUFFER) */

/* Ends the frame StartGraphics began: the text drawn (HAL_FinishText) and the blitter's list ended (BlitFinish) and
 * called from the frame's list, which is closed with a full sync; then fills in the frame record's graphics task
 * (the F3DEX2 microcode, the frame's list, the RSP's stack, yield buffer and RDP FIFO), waits for the RDP only when
 * graphics tasks are still pending, and sends it to the scheduler, its done message to the main thread. The ends of
 * the frame's and the blitter's lists stay in last_frame_dl_end and last_frame_text_dl_end. */
void EndGraphics(void)
{
    Frame *f;
    Gfx *dl;

    f = &frame_records[frame_record_index];
    last_frame_dl_end = frame_dl;
    last_frame_text_dl_end = (intptr_t) blit_dl;
    HAL_FinishText(1);
    dl = BlitFinish();
    gSPDisplayList(frame_dl++, dl);
    gDPFullSync(frame_dl++);
    gSPEndDisplayList(frame_dl++);
    if (PC_FRAME_OVER) {}
    f->t.data_ptr = (u64 *)frame_dynamic->glist;
    f->t.data_size = (u32)((frame_dl - frame_dynamic->glist) * sizeof(Gfx));
    f->t.type = M_GFXTASK;
    f->t.ucode_boot = rspbootTextStart;
    f->t.ucode_boot_size = (u8 *)rspbootTextEnd - (u8 *)rspbootTextStart;
    f->t.ucode_data_size = 2048;
    f->t.dram_stack = rsp_dram_stack;
    f->t.dram_stack_size = 1024;
    if (gfx_tasks_pending == 0) {
        f->t.flags = 0;
    } else {
        f->t.flags = OS_TASK_DP_WAIT;
    }
    if (frame_dl - frame_dynamic->glist > GLIST_LEN) {}
    f->t.ucode = gspF3DEX2_NoN_fifoTextStart;
    f->t.ucode_data = gspF3DEX2_NoN_fifoDataStart;
    f->t.output_buff = rdp_fifo_buffer;
    f->t.output_buff_size = rdp_fifo_buffer_end;
    f->t.yield_data_ptr = rsp_yield_buffer;
    f->t.yield_data_size = 3072;
    f->next = 0;
    f->flags = OS_SC_SWAP_AND_LAST;
    f->msgQ = &main_thread_queue;
    f->msg = &f->doneMsg;
    f->framebuffer = f->buffer;
    osStopThread(scheduler_thread);
    gRenderInProgress++;
    osStartThread(scheduler_thread);
    osSendMesg(&scheduler_task_queue, f, 1); if (pc_fps60) osSendMesg(&scheduler_interrupt_queue, (OSMesg)670, 0);
    gLastFrameCount = gScheduler.retraceCount;
}
