/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * world_collision_response (0x800EDACC, 540 B) = N64 EndGraphics (gfx.c; DC gfx.c func_8C02E390 is the Dreamcast
 * counterpart; MAT3 ngcGraph.c EndGraphics is the GameCube stub). Called by the main-thread message pump
 * (world_trigger_activate) on message 2750: HAL_FinishText(1) (world_velocity_integrate); closes the frame's display
 * list (text DL from audio_start, full sync, end); fills the scheduler task of frame record D_80156BE0[D_8015F72D]
 * (data_ptr/data_size = the Dynamic buffer's glist and its used size, rspboot, gfx ucode, dram stack, output buffer,
 * yield buffer, flags = pending (D_80114730) ? 2 : 0, done-message queue); bumps gRenderInProgress (D_8002EB70) around
 * osStopThread/osStartThread of the scheduler thread, posts the task (osSendMesg) and copies the scheduler's frame count
 * (D_8002E8E8+636) to D_8002EB98.
 * D_8015B250 is the frame's command buffer (SDK "Dynamic"): glist[4000] at +0x9CC0 (the two display buffers
 * D_800586D0 / D_8006A090 are 0x119C0 bytes apart = 0x9CC0 + 4000 * sizeof(Gfx)); D_8015B260 = +1728.
 * (from 88-row draft wcr/best/gfx8_volatile.c). DISCLOSED SHAPING: two compiled-out display-list
 * overflow checks (`if (...) {}`, no code; the release build's error print is gone):
 *   1. `glistp > &dynamicp->glist[GLIST_LEN]` after the end-of-list command: its D_8015B250 load is shared with
 *      data_ptr (retail's t0 web) and splits the block, so &D_8015B250 takes t1 and rspboot v0 as in retail;
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
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef void *OSMesg;
typedef struct { u32 opaque[6]; } OSMesgQueue;
#ifdef TARGET_PC
typedef struct { u32 w0; uintptr_t w1; } Gwords;
#else
typedef struct { u32 w0, w1; } Gwords;
#endif
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
    u8 pad58[4];
    u8 doneMsg[0x7C - 0x5C];/* 0x5C */
    void *buffer;           /* 0x7C */
} Frame;                    /* 0x80 */

extern Gfx *D_801497C8;             /* display-list cursor (msgq_ptr) */
extern volatile s8 D_8015F72D;               /* current frame record */
extern Frame D_80156BE0[];
extern Gfx *D_80156BA8;
extern s32 D_80156BCC;
extern intptr_t D_80149438;
#define GLIST_LEN 4000
typedef struct {
    u8 pad[0x9CC0];
    Gfx glist[GLIST_LEN];
} Dynamic;
extern Dynamic *D_8015B250;
extern s32 D_80114730;
extern u64 D_80027DD0[], D_80027EA0[];   /* rspboot */
extern u64 D_80028CC0[];                 /* gfx ucode text */
extern u64 D_8002DDF0[];                 /* gfx ucode data */
extern u64 D_8007BA50[], D_8007BE50[], D_8007CA50[], D_80084A50[];
extern OSMesgQueue D_8002ECF8;
extern OSMesgQueue D_8002E960;
extern u8 D_8002E998[];                  /* thread */
extern s16 D_8002EB70;
extern volatile s32 D_8002EB98;
typedef struct { u8 pad[636]; s32 frameCount; } Sched;
extern volatile Sched D_8002E8E8;
void world_velocity_integrate(s32);
Gfx *audio_start(void);
void osStopThread(void *);
void osStartThread(void *);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

void world_collision_response(void)
{
    Frame *f;
    Gfx *dl;

    f = &D_80156BE0[D_8015F72D];
    D_80156BA8 = D_801497C8;
    D_80156BCC = D_80149438;
    world_velocity_integrate(1);
    dl = audio_start();
    gSPDisplayList(D_801497C8++, dl);
    gDPFullSync(D_801497C8++);
    gSPEndDisplayList(D_801497C8++);
    if (D_801497C8 > &D_8015B250->glist[GLIST_LEN]) {}
    f->t.data_ptr = (u64 *)D_8015B250->glist;
    f->t.data_size = (u32)((D_801497C8 - D_8015B250->glist) * sizeof(Gfx));
    f->t.type = 1;
    f->t.ucode_boot = D_80027DD0;
    f->t.ucode_boot_size = (u8 *)D_80027EA0 - (u8 *)D_80027DD0;
    f->t.ucode_data_size = 2048;
    f->t.dram_stack = D_8007BA50;
    f->t.dram_stack_size = 1024;
    if (D_80114730 == 0) {
        f->t.flags = 0;
    } else {
        f->t.flags = 2;
    }
    if (D_801497C8 - D_8015B250->glist > GLIST_LEN) {}
    f->t.ucode = D_80028CC0;
    f->t.ucode_data = D_8002DDF0;
    f->t.output_buff = D_8007CA50;
    f->t.output_buff_size = D_80084A50;
    f->t.yield_data_ptr = D_8007BE50;
    f->t.yield_data_size = 3072;
    f->next = 0;
    f->flags = 99;
    f->msgQ = &D_8002ECF8;
    f->msg = &f->doneMsg;
    f->framebuffer = f->buffer;
    osStopThread(D_8002E998);
    D_8002EB70++;
    osStartThread(D_8002E998);
    osSendMesg(&D_8002E960, f, 1);
    D_8002EB98 = D_8002E8E8.frameCount;
}
