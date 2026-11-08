/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * process_scheduler_message (0x800EE5DC, 464 B; historical label) = the N64 main (graphics) thread's message step, called
 * forever by the static main_thread_entry (no GC/DC counterpart: GC/DC flattened the threads). One scheduler message:
 *  - 4 (pre-NMI): sound_freeze_output, video_restore_base_mode, osViBlack(1), modelrun = 0, PS_RumbleStop, pak gInited
 *    (gPakSysInited) = 0, hang;
 *  - 1 (retrace): when the next framebuffer is the current one, no frame is in flight (game_frame_building/70/72), the
 *    scheduler's frame count moved on (+636 != gLastFrameCount) and fewer than two tasks are pending (gfx_tasks_pending):
 *    retrace count retraces_since_frame = 0, StartGraphics, frame_start_pending = 1, wake the game thread
 *    (osSendMesg game_thread_queue); then count++;
 *  - 2750 (frame-end request from the game thread's UpdateFrame): frame_end_requested = 0, EndGraphics
 *    (EndGraphics), toggle the frame record frame_record_index, pending++;
 *  - 2 (task done): toggle D_801161CC (function-local static, own .data), pending--.
 * Base: draft (case order 4, 1, 2750, 2 = retail body layout; volatile flags; function-local static).
 * levers (vbatch b1/b2 + trace): gfx_tasks_pending NOT volatile (address webs v1
 * every case); the pre-NMI and frame-end bodies are inlined helpers (MainPreNMI / MainEndFrame: invented names; with
 * both bodies written in place the frame is 48, retail 56); the switch value is the named local `type` (the 4-byte
 * local above msg, retail sp+52, replacing disclosed unused `x`); the retrace counter goes through the one
 * scratch variable `cur` that also held the framebuffer (`cur = retraces_since_frame; retraces_since_frame = cur + 1;` gives retail's
 * v1 load: a separate local takes v0, force.sh p1:w40=c2 proved it is the only residual). No unused locals, no
 * compiled-out holders. The framebuffer pointers are compared as s32 through `cur`.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef void *OSMesg;
typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef struct { u8 pad[636]; s32 frameCount; } Sched;

extern OSMesgQueue main_thread_queue;
extern OSMesgQueue game_thread_queue;
extern u8 frame_start_msg[];
extern volatile s8 frame_start_pending, game_frame_building, frame_end_requested;
extern volatile Sched gScheduler;
extern volatile s32 gLastFrameCount;
extern u32 gfx_tasks_pending;
extern volatile u16 modelrun;
extern s8 gPakSysInited;
extern volatile s8 frame_record_index;
extern s32 retraces_since_frame;

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
void *osViGetCurrentFramebuffer(void);
void *osViGetNextFramebuffer(void);
void osViBlack(u8);
void sound_freeze_output(void);
void video_restore_base_mode(void);
void PS_RumbleStop(void);
void StartGraphics(void);
void EndGraphics(void);

void MainPreNMI(void)
{
    sound_freeze_output();
    video_restore_base_mode();
    osViBlack(1);
    modelrun = 0;
    PS_RumbleStop();
    gPakSysInited = 0;
    for (;;) {
    }
}

void MainEndFrame(void)
{
    frame_end_requested = 0;
    EndGraphics();
    frame_record_index ^= 1;
    gfx_tasks_pending++;
}


void process_scheduler_message(void)
{
    static s32 D_801161CC = 0;
    s16 type;
    OSMesg msg = 0;
    intptr_t cur;

    osRecvMesg(&main_thread_queue, &msg, 1);
    type = *(s16 *)msg;
    switch (type) {
    case 4:
        MainPreNMI();
        break;
    case 1:
        cur = (intptr_t)osViGetCurrentFramebuffer();
        if ((intptr_t)osViGetNextFramebuffer() == cur && !game_frame_building && !frame_start_pending && !frame_end_requested &&
            gLastFrameCount != gScheduler.frameCount && gfx_tasks_pending < 2) {
            retraces_since_frame = 0;
            StartGraphics();
            frame_start_pending = 1;
            osSendMesg(&game_thread_queue, frame_start_msg, 1);
        }
        cur = retraces_since_frame;
        retraces_since_frame = cur + 1;
        break;
    case 2750:
        MainEndFrame();
        break;
    case 2:
        D_801161CC ^= 1;
        gfx_tasks_pending--;
        break;
    }
}
