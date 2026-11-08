/* GENERATED ROM-aligned TU — segment 0x2cf0 (rom/lib_2cf0)
 * layout map 2fee9198d70b169d9db10078865019d23807b48148a346fda9e88851da0e576b.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "static_debug_context.h"

/* PROMOTED 2026-10-02 — main
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void main(void *argument) {
    u32 index;
    u32 address;
    u32 header[16];
    __osInitialize_common();
    address = 0xFFB000;
    for (index = 0; index < 16; index++, address += 4) {
        osPiReadIo(address, &header[index]);
    }
    osCreateThread(&gIdleThread, 1, idle_thread_entry, argument, gStackIdle + 0x190, 2);
    osStartThread(&gIdleThread);
}

/* PROMOTED 2026-10-02 — idle_thread_entry
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void idle_thread_entry(void *argument) {
    osCreatePiManager(150, &gViModeTable, gViModeLan1, 200);
    osSetThreadPri(NULL, 0);
    osCreateThread(&main_thread, 6, main_thread_entry, argument, main_thread_stack + 0x960, 4);
    osStartThread(&main_thread);
    for (;;) { pc_os_idle(); }
}

/* PROMOTED 2026-10-12 — main_thread_entry
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
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
void main_thread_entry(void *arg) {
    s32 unused0, unused1, unused2, unused3, unused4, unused5; /* frame residual, see header */
    OSScClient client;
    OSMesg msg = 0;
    register s32 tvType;

    __setfpcsr(0x01000E00);
    osCreateMesgQueue(&rom_dma_queue, gDmaMesgBuf, 8);
    osCreateMesgQueue(&unzip_dma_queue, gInflateMesgBuf, 8);
    osCreateMesgQueue(&main_thread_queue, gRetraceMesgBuf, 60);
    osCreateMesgQueue(&game_thread_queue, gEventMesgBuf, 8);
    tvType = video_mode_base_index(), osCreateScheduler(&gScheduler, gSchedulerStack + 0x640, 12, tvType, 1);
    osInvalICache(gDecompressedDataStart, gDecompressedDataEnd1 - gDecompressedDataStart);
    osInvalDCache(gDecompressedDataEnd1, gBssStart - gDecompressedDataEnd1);
    unzipdata_from_rom(gRomCompressedData, gDecompressedDataStart, 0);
    osScAddClient(&gScheduler, &client, &main_thread_queue);
    osCreateThread(&bg_load_thread, 8, BGLoadWaveMain, 0, bg_load_thread_stack + 0x960, 3);
    osCreateMesgQueue(&bg_load_idle_queue, gSyncMesgBuf, 1);
    osSendMesg(&bg_load_idle_queue, 0, 1);
    osStartThread(&bg_load_thread);
    osCreateThread(&model_thread, 5, ModelThread, 0, model_thread_stack + 0x960, 7);
    osStartThread(&model_thread);
    InitSystem();
    BlitInit();
    osCreateThread(&game_thread, 7, (void (*)(void *))game_thread_entry, arg, game_thread_stack + 0x12C0, 5);
    frame_start_msg = 0xABF;
    if (!gInitFlag) {
        osStartThread(&game_thread);
    }
    while (1) {
        osRecvMesg(&main_thread_queue, &msg, 1);
        if (((MainThreadSchedulerMessage *)msg)->type == 0xABE) {
            break;
        }
    }
    frame_end_requested = 0;
    BlitFinish();
    while (1) {
        process_scheduler_message();
    }
}

/* PROMOTED 2026-10-02 — game_thread_entry
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void game_thread_entry(s32 arg) { __setfpcsr(0x01000E00); while(1) { GameLoop(); } }

