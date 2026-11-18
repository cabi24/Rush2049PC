/* The main thread's frame loop (MAT3 game.c's frame pacing; the N64's own): after start-up the main thread serves
 * the scheduler's messages forever (main_thread_entry, src/rom/boot.c). On a retrace with nothing in flight it
 * starts a frame (StartGraphics) and wakes the game thread; when the game thread asks (UpdateFrame) it ends the
 * frame (EndGraphics), whose RSP task the scheduler runs; a finished task frees a slot. At most two frames are in
 * flight. */
#include "game/game.h"


/* The reset button was pressed (the scheduler's pre-NMI message): silences the sound, blacks the screen, stops the
 * car simulation, the rumble and the pak polls, and waits for the reset. */
void MainPreNMI(void)
{
    sound_freeze_output();
    pc_vi_black(1);
    modelrun = 0;
    PS_RumbleStop();
    gPakSysInited = 0;
    for (;;) {
    }
}

/* The game thread has built a frame (GAME_MSG_END_FRAME): ends it (EndGraphics sends its RSP task), switches to the
 * other frame record, and counts the task in flight. */
void MainEndFrame(void)
{
    frame_end_requested = 0;
    EndGraphics();
    frame_record_index ^= 1;
    gfx_tasks_pending++;
}

/* Waits for one message on the main thread's queue and acts on it: pre-NMI (reset), a retrace (start a frame
 * when the screen has shown the last one, no frame is being built, started or ended, gLastFrameCount isn't the
 * retrace count, and fewer than two tasks are in flight; and count the retraces since), the game thread's end of
 * frame, or a finished RSP task.
 * Original game, or a slip in the names: gLastFrameCount is the retraces the last frame took (1-5, UpdateFrameTimer),
 * so it is nearly never the retrace count; gLastFrameCounter, the retrace count at the last frame, would make the
 * test "a retrace has passed since the last frame". */
void process_scheduler_message(void)
{
    static s32 task_done_parity = 0;
    s16 type;
    OSScMsg *msg;
    intptr_t cur;

    msg = queue_recv(&main_thread_queue);
    type = msg->type;
    switch (type) {
    case OS_SC_PRE_NMI_MSG:
        MainPreNMI();
        break;
    case OS_SC_RETRACE_MSG:
        cur = (intptr_t)pc_vi_current_framebuffer();
        if ((intptr_t)pc_vi_next_framebuffer() == cur && !game_frame_building && !frame_start_pending && !frame_end_requested &&
            gLastFrameCount != gScheduler.retraceCount && gfx_tasks_pending < 2) {
            retraces_since_frame = 0;
            StartGraphics();
            frame_start_pending = 1;
            queue_send(&game_thread_queue, &frame_start_msg);
        }
        cur = retraces_since_frame;
        retraces_since_frame = cur + 1;
        break;
    case GAME_MSG_END_FRAME:
        MainEndFrame();
        break;
    case OS_SC_DONE_MSG:
        task_done_parity ^= 1;
        gfx_tasks_pending--;
        break;
    }
}
