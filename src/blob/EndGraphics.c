/* EndGraphics: end a frame's display list and hand it to the scheduler (MAT3 gfx.c; the N64 side of it). */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union { Gwords words; long long force_structure_alignment; } Gfx;
#define gDma1p_(pkt, c, s) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = (c); _g->words.w1 = (uintptr_t)(s); }
#define gSPDisplayList(pkt, dl) gDma1p_(pkt, 0xDE000000, dl)
#define gDPFullSync(pkt) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = 0xE9000000; _g->words.w1 = 0; }
#define gSPEndDisplayList(pkt) { Gfx *_g = (Gfx *)(pkt); _g->words.w0 = 0xDF000000; _g->words.w1 = 0; }

VIEW(Gfx *, frame_dl);
#define frame_dl frame_dl_view

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

/* PC, game.frame_rate 60 (docs/port/FRAMERATE.md 7): the frame's task waits in the scheduler's task queue until a
 * command on its event queue wakes it (sched_main, SCHED_CMD_SCHEDULE). In real time on the PC the RSP finishes the
 * retrace's audio task at once, before the frame is done, so the next message was the next retrace: the frame's
 * task ran a retrace late and was shown two after the frame began, every frame. At 60 the frame's task wakes the
 * scheduler itself, as the audio task's sender does (musyx_submit_audio_task). Original keeps the N64's messages. */

#define M_GFXTASK 1                 /* OSTask type: a graphics task */
#define OS_TASK_DP_WAIT 2           /* OSTask flag: wait for the RDP to finish the last task's output */

/* Ends the frame StartGraphics began: the text drawn (HAL_FinishText) and the blitter's list ended (BlitFinish) and
 * called from the frame's list, which is closed with a full sync; then fills in the frame record's graphics task
 * (the F3DEX2 microcode, the frame's list, the RSP's stack, yield buffer and RDP FIFO), waits for the RDP only when
 * graphics tasks are still pending, and sends it to the scheduler, its done message to the main thread. The ends of
 * the frame's and the blitter's lists stay in last_frame_dl_end and last_frame_text_dl_end. */
void EndGraphics(void)
{
    FrameRecord *f;
    Gfx *dl;

    f = &frame_records[frame_record_index];
    last_frame_dl_end = frame_dl;
    last_frame_text_dl_end = (void *)blit_dl;
    HAL_FinishText(1);
    dl = BlitFinish();
    gSPDisplayList(frame_dl++, dl);
    gDPFullSync(frame_dl++);
    gSPEndDisplayList(frame_dl++);
    if (PC_FRAME_OVER) {}
    f->task.list.data_ptr = (u64 *)frame_dynamic->glist;
    f->task.list.data_size = (u32)((frame_dl - frame_dynamic->glist) * sizeof(Gfx));
    f->task.list.type = M_GFXTASK;
    f->task.list.ucode_boot = rspbootTextStart;
    f->task.list.ucode_boot_size = (u8 *)rspbootTextEnd - (u8 *)rspbootTextStart;
    f->task.list.ucode_data_size = 2048;
    f->task.list.dram_stack = rsp_dram_stack;
    f->task.list.dram_stack_size = 1024;
    if (gfx_tasks_pending == 0) {
        f->task.list.flags = 0;
    } else {
        f->task.list.flags = OS_TASK_DP_WAIT;
    }
    if (frame_dl - frame_dynamic->glist > GLIST_LEN) {}
    f->task.list.ucode = gspF3DEX2_NoN_fifoTextStart;
    f->task.list.ucode_data = gspF3DEX2_NoN_fifoDataStart;
    f->task.list.output_buff = rdp_fifo_buffer;
    f->task.list.output_buff_size = rdp_fifo_buffer_end;
    f->task.list.yield_data_ptr = rsp_yield_buffer;
    f->task.list.yield_data_size = 3072;
    f->task.next = 0;
    f->task.flags = OS_SC_NEEDS_RDP | OS_SC_NEEDS_RSP | OS_SC_LAST_TASK | OS_SC_SWAPBUFFER;
    f->task.msgQueue = &main_thread_queue;
    f->task.msg = &f->doneMsg;
    f->task.framebuffer = f->colorBuffer;
    thread_stop(&gScheduler.thread);
    gRenderInProgress++;
    thread_start(&gScheduler.thread);
    queue_send(&gScheduler.taskQueue, &f->task);
    if (pc_fps60) queue_try_send(&gScheduler.eventQueue, MESG_NUMBER(SCHED_CMD_SCHEDULE));
    gLastFrameCount = gScheduler.retraceCount;
}
