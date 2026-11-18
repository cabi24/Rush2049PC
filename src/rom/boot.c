/* The game's start-up and its first threads (the boot segment; MAT3's start is its GameCube main): main starts
 * libultra and the idle thread; the idle thread starts the main thread; the main thread makes the scheduler, starts
 * the background loader and the car simulation, runs InitSystem, starts the game thread, and then serves the
 * scheduler's messages forever (process_scheduler_message). On the PC the threads are host threads, and the game
 * image, which the N64's main thread inflated from the cartridge, is part of the program (its data port/data's). The
 * threads' ids and priorities are game/threads.h's THREAD_*. */
#include "static_debug_context.h"   /* rom_tu.h, and with it game/threads.h */

/* The game threads' FPU control: flush denormals to zero (FS) and trap invalid operations, divide by zero and
 * overflow. port/ultra records it. */
#define FPCSR_GAME 0x01000E00

/* The N64's entry point (libultra's boot calls it): starts libultra and the idle thread, which runs everything else.
 * (The N64's also read 64 bytes of the cartridge, past the ROM's end, that nothing used.) */
void main(void *argument) {
    __osInitialize_common();
    thread_create(&gIdleThread, THREAD_ID_IDLE, idle_thread_entry, argument, gStackIdle + 0x190, THREAD_PRI_IDLE_START);
    thread_start(&gIdleThread);
}

/* The idle thread: drops to priority 0, starts the main thread and then idles for good. (The N64's started the
 * cartridge's DMA manager first; the PC reads the cartridge at once, pc_cart_read.) */
void idle_thread_entry(void *argument) {
    thread_set_priority(NULL, THREAD_PRI_IDLE);
    thread_create(&main_thread, THREAD_ID_MAIN, main_thread_entry, argument, main_thread_stack + 0x960, THREAD_PRI_MAIN);
    thread_start(&main_thread);
    for (;;) { pc_os_idle(); }
}

extern OSMesgQueue rom_dma_queue, unzip_dma_queue, main_thread_queue, game_thread_queue, bg_load_idle_queue;
extern OSMesg gDmaMesgBuf[], gInflateMesgBuf[], gRetraceMesgBuf[], gEventMesgBuf[], gSyncMesgBuf[];
extern u8 gSchedulerStack[], bg_load_thread_stack[], model_thread_stack[], game_thread_stack[];
extern OSThread bg_load_thread, model_thread, game_thread;
extern s16 frame_start_msg;
extern s8 gInitFlag;
extern s8 frame_end_requested;
extern void __setfpcsr(u32 value);
extern void bzero(void *s, s32 n);
extern s32 video_mode_base_index(void);
extern void BGLoadWaveMain(void *);
extern void ModelThread(void *);
extern void game_thread_entry(s32);
extern void InitSystem(void);
extern void BlitInit(void);
extern void BlitFinish(void);
extern void process_scheduler_message(void);

/* The main thread: the FPU mode; the queues (cartridge reads, inflate DMA, the scheduler's messages to this thread,
 * the frame-start messages to the game thread); the scheduler (one field a retrace, the TV's video mode); the
 * background loader (idle at first) and the car simulation started; InitSystem; the boot screen (BlitInit) and the game thread, started unless gInitFlag holds it. Then it waits for
 * the game thread's first end of frame, ends the boot screen and serves the scheduler's messages forever. */
void main_thread_entry(void *arg) {
    OSScClient client;
    register s32 tvType;

    __setfpcsr(FPCSR_GAME);
    queue_init(&rom_dma_queue, gDmaMesgBuf, 8);
    queue_init(&unzip_dma_queue, gInflateMesgBuf, 8);
    queue_init(&main_thread_queue, gRetraceMesgBuf, 60);
    queue_init(&game_thread_queue, gEventMesgBuf, 8);
    tvType = video_mode_base_index(), sched_create(&gScheduler, gSchedulerStack + 0x640, THREAD_PRI_SCHEDULER, tvType, 1);
    sched_add_client(&gScheduler, &client, &main_thread_queue);
    thread_create(&bg_load_thread, THREAD_ID_BG_LOAD, BGLoadWaveMain, 0, bg_load_thread_stack + 0x960, THREAD_PRI_BG_LOAD);
    queue_init(&bg_load_idle_queue, gSyncMesgBuf, 1);
    queue_send(&bg_load_idle_queue, NULL);   /* the loader's idle token */
    thread_start(&bg_load_thread);
    thread_create(&model_thread, THREAD_ID_MODEL, ModelThread, 0, model_thread_stack + 0x960, THREAD_PRI_MODEL);
    thread_start(&model_thread);
    InitSystem();
    BlitInit();
    thread_create(&game_thread, THREAD_ID_GAME, (void (*)(void *))game_thread_entry, arg, game_thread_stack + 0x12C0, THREAD_PRI_GAME);
    frame_start_msg = GAME_MSG_START_FRAME;
    if (!gInitFlag) {
        thread_start(&game_thread);
    }
    while (1) {
        OSScMsg *msg = queue_recv(&main_thread_queue);
        if (msg->type == GAME_MSG_END_FRAME) {
            break;
        }
    }
    frame_end_requested = 0;
    BlitFinish();
    while (1) {
        process_scheduler_message();
    }
}

/* The game thread: the FPU mode, then GameLoop forever (a frame each call). */
void game_thread_entry(s32 arg) { __setfpcsr(FPCSR_GAME); while(1) { GameLoop(); } }
