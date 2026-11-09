/* The game's start-up and its first threads (the boot segment; MAT3's start is its GameCube main): main starts
 * libultra and the idle thread; the idle thread starts the PI manager and the main thread; the main thread makes
 * the scheduler, inflates the game image, starts the background loader and the car simulation, runs InitSystem,
 * starts the game thread, and then serves the scheduler's messages forever (process_scheduler_message). On the PC
 * the threads are host threads and the inflate fills the RDRAM window from the cartridge (port/ultra). The
 * threads' ids and priorities are system.h's THREAD_*. */
#include "static_debug_context.h"

/* The game threads' FPU control: flush denormals to zero (FS) and trap invalid operations, divide by zero and
 * overflow. port/ultra records it. */
#define FPCSR_GAME 0x01000E00
/* The thread ids and priorities (include/game/system.h, which this boot code doesn't include). */
#define THREAD_ID_IDLE 1
#define THREAD_ID_MODEL 5
#define THREAD_ID_MAIN 6
#define THREAD_ID_GAME 7
#define THREAD_ID_BG_LOAD 8
#define THREAD_PRI_IDLE_START 2
#define THREAD_PRI_BG_LOAD 3
#define THREAD_PRI_MAIN 4
#define THREAD_PRI_GAME 5
#define THREAD_PRI_MODEL 7
#define THREAD_PRI_SCHEDULER 12
#define THREAD_PRI_PI_MANAGER 150
/* The game thread's message asking the main thread to end a frame, and the main thread's to start one. */
#define GAME_MSG_END_FRAME 0xABE
#define GAME_MSG_START_FRAME 0xABF

/* The N64's entry point (libultra's boot calls it): starts libultra, reads the cartridge header's first 64 bytes
 * (unused), and starts the idle thread, which runs everything else. */
void main(void *argument) {
    u32 index;
    u32 address;
    u32 header[16];
    __osInitialize_common();
    address = 0xFFB000;
    for (index = 0; index < 16; index++, address += 4) {
        osPiReadIo(address, &header[index]);
    }
    osCreateThread(&gIdleThread, THREAD_ID_IDLE, idle_thread_entry, argument, gStackIdle + 0x190, THREAD_PRI_IDLE_START);
    osStartThread(&gIdleThread);
}

/* The idle thread: starts the PI manager (the cartridge's DMA), drops to priority 0, starts the main thread and then
 * idles for good. */
void idle_thread_entry(void *argument) {
    osCreatePiManager(THREAD_PRI_PI_MANAGER, &gViModeTable, gViModeLan1, 200);
    osSetThreadPri(NULL, 0);
    osCreateThread(&main_thread, THREAD_ID_MAIN, main_thread_entry, argument, main_thread_stack + 0x960, THREAD_PRI_MAIN);
    osStartThread(&main_thread);
    for (;;) { pc_os_idle(); }
}

typedef struct { s16 type; } MainThreadSchedulerMessage;
typedef struct MainThreadScheduler MainThreadScheduler;
extern OSMesgQueue rom_dma_queue, unzip_dma_queue, main_thread_queue, game_thread_queue, bg_load_idle_queue;
extern OSMesg gDmaMesgBuf[], gInflateMesgBuf[], gRetraceMesgBuf[], gEventMesgBuf[], gSyncMesgBuf[];
extern MainThreadScheduler gScheduler;
extern u8 gSchedulerStack[], bg_load_thread_stack[], model_thread_stack[], game_thread_stack[];
extern u8 gDecompressedDataStart[], gDecompressedDataEnd1[], gBssStart[], game_bss_end[], gRomCompressedData[];
extern OSThread bg_load_thread, model_thread, game_thread;
extern s16 frame_start_msg;
extern s8 gInitFlag;
extern s8 frame_end_requested;
extern void __setfpcsr(u32 value);
extern void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
extern s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
extern s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
extern void osCreateThread(OSThread *thread, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri pri);
extern void osStartThread(OSThread *thread);
extern void osCreateScheduler(MainThreadScheduler *, void *, OSPri, u8, u8);
extern void osScAddClient(MainThreadScheduler *, OSScClient *, OSMesgQueue *);
extern void osInvalICache(void *vaddr, s32 nbytes);
extern void osInvalDCache(void *vaddr, s32 nbytes);
extern void bzero(void *s, s32 n);
extern s32 unzipdata_from_rom(void *, void *, s32);
extern s32 video_mode_base_index(void);
extern void BGLoadWaveMain(void *);
extern void ModelThread(void *);
extern void game_thread_entry(s32);
extern void InitSystem(void);
extern void BlitInit(void);
extern void BlitFinish(void);
extern void process_scheduler_message(void);

/* The main thread: the FPU mode; the queues (cartridge DMA, inflate DMA, the scheduler's messages to this thread,
 * the frame-start messages to the game thread); the scheduler (one field a retrace, the TV's video mode); the game
 * image inflated from the cartridge; the background loader (idle at first) and the car simulation started;
 * InitSystem; the boot screen (BlitInit) and the game thread, started unless gInitFlag holds it. Then it waits for
 * the game thread's first end of frame, ends the boot screen and serves the scheduler's messages forever. */
void main_thread_entry(void *arg) {
    OSScClient client;
    OSMesg msg = 0;
    register s32 tvType;

    __setfpcsr(FPCSR_GAME);
    osCreateMesgQueue(&rom_dma_queue, gDmaMesgBuf, 8);
    osCreateMesgQueue(&unzip_dma_queue, gInflateMesgBuf, 8);
    osCreateMesgQueue(&main_thread_queue, gRetraceMesgBuf, 60);
    osCreateMesgQueue(&game_thread_queue, gEventMesgBuf, 8);
    tvType = video_mode_base_index(), osCreateScheduler(&gScheduler, gSchedulerStack + 0x640, THREAD_PRI_SCHEDULER, tvType, 1);
    osInvalICache(gDecompressedDataStart, gDecompressedDataEnd1 - gDecompressedDataStart);
    osInvalDCache(gDecompressedDataEnd1, gBssStart - gDecompressedDataEnd1);
    unzipdata_from_rom(gRomCompressedData, gDecompressedDataStart, 0);
    osScAddClient(&gScheduler, &client, &main_thread_queue);
    osCreateThread(&bg_load_thread, THREAD_ID_BG_LOAD, BGLoadWaveMain, 0, bg_load_thread_stack + 0x960, THREAD_PRI_BG_LOAD);
    osCreateMesgQueue(&bg_load_idle_queue, gSyncMesgBuf, 1);
    osSendMesg(&bg_load_idle_queue, 0, 1);
    osStartThread(&bg_load_thread);
    osCreateThread(&model_thread, THREAD_ID_MODEL, ModelThread, 0, model_thread_stack + 0x960, THREAD_PRI_MODEL);
    osStartThread(&model_thread);
    InitSystem();
    BlitInit();
    osCreateThread(&game_thread, THREAD_ID_GAME, (void (*)(void *))game_thread_entry, arg, game_thread_stack + 0x12C0, THREAD_PRI_GAME);
    frame_start_msg = GAME_MSG_START_FRAME;
    if (!gInitFlag) {
        osStartThread(&game_thread);
    }
    while (1) {
        osRecvMesg(&main_thread_queue, &msg, 1);
        if (((MainThreadSchedulerMessage *)msg)->type == GAME_MSG_END_FRAME) {
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
