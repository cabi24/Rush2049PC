/* port/ultra/selftest.c: rush2049pc --selftest. Exercises the PC libultra without the ROM: thread priorities
 * and preemption, blocking and non-blocking message calls, jam, stop/start, osSetThreadPri, yield, events,
 * the VI retrace, timers, busy-wait yields, the RDRAM window and big-endian ranges, and the PIF with the
 * game's own osContInit / osContStartReadData / osPfsInitPak C (src/rom). Runs on the real main loop; with
 * --fast, on the virtual clock (timer.c), where the same bounds hold and the whole run takes a fraction of the
 * time. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ultra.h"

/* port/ultra and the game's libultra C, as the game declares them. */
void osCreateThread(void *t, int32_t id, void (*entry)(void *), void *arg, void *sp, int32_t pri);
void osStartThread(void *t);
void osStopThread(void *t);
void osYieldThread(void);
void osSetThreadPri(void *t, int32_t pri);
int32_t osGetThreadPri(void *t);
void osCreateMesgQueue(void *mq, OSMesg *msg, int32_t count);
int32_t osSendMesg(void *mq, OSMesg msg, int32_t flags);
int32_t osJamMesg(void *mq, OSMesg msg, int32_t flags);
int32_t osRecvMesg(void *mq, OSMesg *msg, int32_t flags);
void osSetEventMesg(uint32_t e, void *mq, OSMesg msg);
void osViSetEvent(void *mq, OSMesg msg, uint32_t count);
int32_t osSetTimer(void *t, uint64_t countdown, uint64_t interval, void *mq, OSMesg msg);
int32_t osStopTimer(void *t);
uint64_t osGetTime(void);
int32_t osContInit(void *mq, uint8_t *bits, void *status);
int32_t osContStartReadData(void *mq);
void osContGetReadData(void *pads);
int32_t osPfsInitPak(void *mq, void *pfs, int32_t channel);
int32_t osPfsFreeBlocks(void *pfs, int32_t *bytes);
int32_t osPfsAllocateFile(void *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext, int32_t size,
                          int32_t *file);
int32_t osPfsReadWriteFile(void *pfs, int32_t file, uint8_t flag, int32_t offset, int32_t size, uint8_t *buf);
int32_t osPfsDeleteFile(void *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext);
void osSpTaskLoad(void *task);
void osSpTaskStartGo(void *task);
int32_t osDpSetNextBuffer(void *buf, uint64_t size);
int32_t osAiSetFrequency(uint32_t hz);
int32_t osAiSetNextBuffer(void *buf, uint32_t len);

/* The game's names at absolute N64 addresses (gen_stubs, aliases.ld), and data in the arenas at their N64
 * addresses (port/data: boot data gWaveSizes, game data, runtime image A's BSS). */
extern uint8_t D_8017A640[], D_8038A400[];
extern uint32_t osTvType, D_80000318;
extern uint8_t D_8002E580[], D_8011B5BC[], D_803B9A50[];

#define MS(n) ((uint64_t)(n) * PC_COUNT_HZ / 1000)
/* How late a loaded host (parallel ctest, CI runners) may wake a thread. Lower bounds stay exact: nothing may
 * fire early. */
#define SLACK_MS 250

static uint8_t g_thread[32][32];   /* stand-ins for the game's OSThread storage: only their addresses matter */
static int g_nthread;
static uint8_t g_queue[16][24];
static OSMesg g_buf[16][8];
static uint8_t g_timer[4][48];
static char g_log[512];
static int g_failed, g_run;
static void *Q;

static void lg(const char *s)
{
    if (g_log[0])
        strcat(g_log, " ");
    strcat(g_log, s);
}

static void *thread(void (*entry)(void *), void *arg, int32_t pri)
{
    void *t = g_thread[g_nthread++];
    osCreateThread(t, 100 + g_nthread, entry, arg, NULL, pri);
    return t;
}

static void *queue(int i, int32_t count)
{
    osCreateMesgQueue(g_queue[i], g_buf[i], count);
    return g_queue[i];
}

static void check(const char *name, int ok, const char *detail)
{
    g_run++;
    if (!ok)
        g_failed++;
    fprintf(stderr, "selftest: %-28s %s%s%s [real %.1f ms, n64 %.1f ms]\n", name, ok ? "ok" : "FAIL",
            detail && !ok ? ": " : "", detail && !ok ? detail : "", pc_real_ns() / 1e6,
            pc_host_ns() / 1e6);   /* TEMPORARY diagnostic */
}

static void expect_log(const char *name, const char *want)
{
    char detail[700];
    snprintf(detail, sizeof detail, "got \"%s\", want \"%s\"", g_log, want);
    check(name, !strcmp(g_log, want), detail);
    g_log[0] = 0;
}

/* The host's clock, or with --fast the virtual one that stands in for it. */
static double host_ms(void)
{
    return pc_host_ns() / 1e6;
}

static void sleep_ms(int ms)
{
    void *q = queue(15, 1);
    osSetTimer(g_timer[3], MS(ms), 0, q, NULL);
    osRecvMesg(q, NULL, OS_MESG_BLOCK);
}

/* ---------------------------------------------------------------- workers */

static void w_log(void *s) { lg(s); }
static void w_low(void *arg)
{
    (void)arg;
    lg("L1");
    osSendMesg(Q, (OSMesg)1, OS_MESG_BLOCK);
    lg("L2");
}
static void w_sender(void *arg)
{
    (void)arg;
    osSendMesg(Q, (OSMesg)1, OS_MESG_BLOCK);
    osSendMesg(Q, (OSMesg)2, OS_MESG_BLOCK);
    lg("S2done");
}
static void w_yield(void *name)
{
    char a[8], b[8];
    snprintf(a, sizeof a, "%sa", (char *)name);
    snprintf(b, sizeof b, "%sb", (char *)name);
    lg(a);
    osYieldThread();
    lg(b);
    if (!strcmp(name, "E2"))
        osSendMesg(Q, NULL, OS_MESG_BLOCK);
}
static void w_waiter(void *arg)
{
    (void)arg;
    OSMesg m;
    osRecvMesg(Q, &m, OS_MESG_BLOCK);
    char s[16];
    snprintf(s, sizeof s, "Wgot%d", (int)(intptr_t)m);
    lg(s);
}
static volatile int g_flag;
static void w_setter(void *arg)
{
    (void)arg;
    void *q = queue(14, 1);
    osSetTimer(g_timer[2], MS(20), 0, q, NULL);
    osRecvMesg(q, NULL, OS_MESG_BLOCK);
    g_flag = 1;
}
static void w_set_flag(void *arg) { (void)arg; g_flag = 2; }

/* ---------------------------------------------------------------- tests */

static void t_priorities(void)
{
    Q = queue(0, 1);
    void *lo = thread(w_low, NULL, 10), *hi = thread(w_log, "H", 60);
    OSMesg m = 0;
    lg("s1");
    osStartThread(lo);   /* lower: keeps running */
    lg("s2");
    osStartThread(hi);   /* higher: runs at once */
    lg("s3");
    osRecvMesg(Q, &m, OS_MESG_BLOCK);   /* blocks: the low thread runs and wakes us */
    lg("got");
    expect_log("priorities and preemption", "s1 s2 H s3 L1 got");
    sleep_ms(5);   /* lets the low thread finish */
    g_log[0] = 0;
}

static void t_queue(void)
{
    void *q = queue(2, 4);
    OSMesg m;
    int r[8], k = 0;
    osSendMesg(q, (OSMesg)1, OS_MESG_NOBLOCK);
    osSendMesg(q, (OSMesg)2, OS_MESG_NOBLOCK);
    osSendMesg(q, (OSMesg)3, OS_MESG_NOBLOCK);
    osJamMesg(q, (OSMesg)9, OS_MESG_NOBLOCK);
    int full = osSendMesg(q, (OSMesg)5, OS_MESG_NOBLOCK);
    while (osRecvMesg(q, &m, OS_MESG_NOBLOCK) == 0 && k < 8)
        r[k++] = (int)(intptr_t)m;
    char d[64];
    snprintf(d, sizeof d, "full %d, got %d values: %d %d %d %d", full, k, r[0], r[1], r[2], r[3]);
    check("FIFO, jam, full, empty", full == -1 && k == 4 && r[0] == 9 && r[1] == 1 && r[2] == 2 && r[3] == 3, d);
}

static void t_blocking_send(void)
{
    Q = queue(3, 1);
    OSMesg a = 0, b = 0;
    osStartThread(thread(w_sender, NULL, 60));   /* fills the queue, then blocks sending 2 */
    lg("c1");
    osRecvMesg(Q, &a, OS_MESG_BLOCK);   /* frees a slot: the sender finishes at once */
    osRecvMesg(Q, &b, OS_MESG_BLOCK);
    char d[32];
    snprintf(d, sizeof d, "values %d %d", (int)(intptr_t)a, (int)(intptr_t)b);
    check("blocking send values", a == (OSMesg)1 && b == (OSMesg)2, d);
    expect_log("blocking send order", "c1 S2done");
}

static void t_yield(void)
{
    Q = queue(4, 1);
    osStartThread(thread(w_yield, "E1", 30));
    osStartThread(thread(w_yield, "E2", 30));
    osRecvMesg(Q, NULL, OS_MESG_BLOCK);
    expect_log("equal priorities, yield", "E1a E2a E1b E2b");
}

static void t_setpri(void)
{
    void *p = thread(w_log, "P", 40);
    osStartThread(p);
    osSetThreadPri(p, 60);   /* now higher than us: runs */
    lg("after");
    osStartThread(thread(w_log, "Q1", 45));
    osSetThreadPri(NULL, 40);   /* we drop below it: it runs */
    lg("back");
    osSetThreadPri(NULL, 50);
    expect_log("osSetThreadPri", "P after Q1 back");
    check("osGetThreadPri", osGetThreadPri(NULL) == 50, NULL);
}

static void t_stop(void)
{
    Q = queue(5, 1);
    void *w = thread(w_waiter, NULL, 60);
    osStartThread(w);   /* blocks on Q */
    osStopThread(w);
    osSendMesg(Q, (OSMesg)5, OS_MESG_NOBLOCK);   /* a stopped thread is not woken */
    lg("sent");
    osStartThread(w);
    expect_log("osStopThread while waiting", "sent Wgot5");
}

static void t_events(void)
{
    void *q = queue(6, 8);
    OSMesg m = 0;
    osSetEventMesg(OS_EVENT_SW2, q, (OSMesg)0x42);
    pc_os_event(OS_EVENT_SW2);
    int r = osRecvMesg(q, &m, OS_MESG_NOBLOCK);
    check("osSetEventMesg", r == 0 && m == (OSMesg)0x42, NULL);
    osSetEventMesg(OS_EVENT_SW2, NULL, NULL);

    uint64_t t0 = osGetTime();
    osViSetEvent(q, (OSMesg)0x66, 1);
    int n = 0;
    while (n < 12 && osRecvMesg(q, &m, OS_MESG_BLOCK) == 0 && m == (OSMesg)0x66)
        n++;
    osViSetEvent(NULL, NULL, 1);
    double ms = (osGetTime() - t0) * 1000.0 / PC_COUNT_HZ;
    char d[64];
    snprintf(d, sizeof d, "%d retraces in %.0f ms", n, ms);
    check("VI retrace at 60 Hz", n == 12 && ms > 150 && ms < 200 + SLACK_MS, d);
}

static void t_timers(void)
{
    void *q = queue(7, 8);
    OSMesg m;
    uint64_t t0 = osGetTime();
    osSetTimer(g_timer[0], MS(50), 0, q, (OSMesg)1);
    osRecvMesg(q, &m, OS_MESG_BLOCK);
    double ms = (osGetTime() - t0) * 1000.0 / PC_COUNT_HZ;
    char d[64];
    snprintf(d, sizeof d, "fired after %.1f ms", ms);
    check("one-shot timer", m == (OSMesg)1 && ms >= 49 && ms < 50 + SLACK_MS, d);

    t0 = osGetTime();
    osSetTimer(g_timer[1], MS(10), MS(10), q, (OSMesg)2);
    for (int i = 0; i < 5; i++)
        osRecvMesg(q, &m, OS_MESG_BLOCK);
    ms = (osGetTime() - t0) * 1000.0 / PC_COUNT_HZ;
    int stopped = osStopTimer(g_timer[1]);
    while (osRecvMesg(q, &m, OS_MESG_NOBLOCK) == 0) {}
    sleep_ms(30);
    int late = osRecvMesg(q, &m, OS_MESG_NOBLOCK) == 0;
    snprintf(d, sizeof d, "5 periods in %.1f ms, stop %d, late %d", ms, stopped, late);
    check("periodic timer, osStopTimer", ms >= 49 && ms < 50 + SLACK_MS && stopped == 0 && !late, d);

    /* Against the host clock, so a late wake-up doesn't count. */
    double h0 = host_ms();
    t0 = osGetTime();
    sleep_ms(100);
    double ticks = (double)(osGetTime() - t0), host = host_ms() - h0, err = ticks / host / 46875.0 - 1;
    snprintf(d, sizeof d, "%.0f ticks in %.1f ms", ticks, host);
    check("osGetTime at 46.875 MHz", host >= 99 && err > -0.02 && err < 0.02, d);
}

static void t_spin(void)
{
    uint64_t t0 = osGetTime();
    g_flag = 0;
    osStartThread(thread(w_setter, NULL, 60));   /* waits on a timer, then sets the flag */
    while (g_flag != 1)
        pc_os_spin_yield("selftest spin");
    double ms = (osGetTime() - t0) * 1000.0 / PC_COUNT_HZ;
    char d[64];
    snprintf(d, sizeof d, "%.1f ms", ms);
    check("busy-wait, higher writer", ms < 150, d);   /* below the lower writer's fallback */

    t0 = osGetTime();
    osStartThread(thread(w_set_flag, NULL, 10));   /* lower priority: only the fallback runs it */
    while (g_flag != 2)
        pc_os_spin_yield("selftest spin");
    ms = (osGetTime() - t0) * 1000.0 / PC_COUNT_HZ;
    snprintf(d, sizeof d, "%.1f ms", ms);
    check("busy-wait, lower writer", ms > 150 && ms < 2000, d);
}

static void t_memory(void)
{
    char d[96];
    int fixed = pc_mem_fixed();
    uint8_t *k0 = pc_n64_ptr(0x80001000), *k1 = pc_n64_ptr(0xA0001000), *ph = pc_n64_ptr(0x00001000);
    k0[0] = 0x5A;
    snprintf(d, sizeof d, "fixed %d, K0 %p, KSEG1 %p, physical %p", fixed, (void *)k0, (void *)k1, (void *)ph);
    check("RDRAM window", k0 == k1 && k0 == ph && pc_n64_addr(k0) == 0x80001000 && pc_n64_ptr(0x00800000) == NULL &&
                              (!fixed || (k0 == (uint8_t *)0x80001000 && *(volatile uint8_t *)0xA0001000 == 0x5A)),
          d);
    snprintf(d, sizeof d, "heap %p, image base %p, osTvType %p = %u, osMemSize 0x%X", (void *)D_8017A640,
             (void *)D_8038A400, (void *)&osTvType, osTvType, D_80000318);
    check("absolute N64 symbols", (uintptr_t)D_8017A640 == 0x8017A640u && (uintptr_t)D_8038A400 == 0x8038A400u &&
                                      (uintptr_t)&osTvType == 0x80000300u && osTvType == 1 &&
                                      D_80000318 == pc_os_mem_size(), d);
    snprintf(d, sizeof d, "%p, %p, %p", (void *)D_8002E580, (void *)D_8011B5BC, (void *)D_803B9A50);
    check("game data at its N64 addresses", (uintptr_t)D_8002E580 == 0x8002E580u &&
                                                (uintptr_t)D_8011B5BC == 0x8011B5BCu &&
                                                (uintptr_t)D_803B9A50 == 0x803B9A50u, d);
    int local;
    snprintf(d, sizeof d, "code %p, data %p, stack %p", (void *)check, (void *)g_log, (void *)&local);
    check("host addresses above 4 GiB", (uintptr_t)check >> 32 && (uintptr_t)g_log >> 32 && (uintptr_t)&local >> 32,
          d);

    static uint8_t buf[256];
    pc_mem_register_be(buf, 100);
    pc_mem_register_be(buf + 50, 100);   /* overlaps: one range 0-150 */
    pc_mem_register_be(buf + 10, 20);    /* inside: no change */
    pc_mem_register_be(buf + 200, 10);
    pc_mem_register_be(buf + 210, 10);   /* adjacent: its own range */
    int ok = pc_mem_is_be(buf) && pc_mem_is_be(buf + 149) && !pc_mem_is_be(buf + 150) && pc_mem_is_be(buf + 205) &&
             pc_mem_is_be(buf + 219) && !pc_mem_is_be(buf + 220);
    pc_mem_unregister_be(buf + 60);      /* drops all of 0-150 */
    ok = ok && !pc_mem_is_be(buf) && !pc_mem_is_be(buf + 120) && pc_mem_is_be(buf + 200);
    pc_mem_unregister_be(buf + 200);
    ok = ok && !pc_mem_is_be(buf + 200) && pc_mem_is_be(buf + 210);
    pc_mem_unregister_be(buf + 210);
    check("big-endian ranges", ok && !pc_mem_is_be(buf), NULL);
}

static void t_pif(void)
{
    void *si = queue(8, 1);
    uint8_t bits = 0, status[4][4] = {{0}};
    uint8_t pads[4][6] = {{0}};
    static uint8_t pfs[1024];
    int32_t bytes = -1;
    osSetEventMesg(OS_EVENT_SI, si, NULL);
    int32_t r = osContInit(si, &bits, status);
    uint16_t type;
    memcpy(&type, status[0], 2);
    char d[96];
    snprintf(d, sizeof d, "ret %d, pattern 0x%02X, type 0x%04X, status 0x%02X", (int)r, bits, type, status[0][2]);
    check("osContInit (game C) via PIF", r == 0 && bits == 0x01 && type == 0x0005 && status[0][2] == 0x01, d);

    PcPad p[4] = {{0x9000, 12, -34, 1}};   /* A + START */
    pc_si_set_pads(p);
    osContStartReadData(si);
    osRecvMesg(si, NULL, OS_MESG_BLOCK);
    osContGetReadData(pads);
    uint16_t buttons;
    memcpy(&buttons, pads[0], 2);
    snprintf(d, sizeof d, "buttons 0x%04X, stick %d,%d, error %d", buttons, (int8_t)pads[0][2], (int8_t)pads[0][3],
             pads[0][4]);
    check("osContGetReadData (game C)", buttons == 0x9000 && (int8_t)pads[0][2] == 12 &&
                                            (int8_t)pads[0][3] == -34 && pads[0][4] == 0, d);
    PcPad none[4] = {{0, 0, 0, 1}};
    pc_si_set_pads(none);

    r = osPfsInitPak(si, pfs, 0);
    int32_t r2 = r == 0 ? osPfsFreeBlocks(pfs, &bytes) : -1;
    snprintf(d, sizeof d, "osPfsInitPak %d, osPfsFreeBlocks %d: %d bytes free", (int)r, (int)r2, (int)bytes);
    check("controller pak (game Pfs C)", r == 0 && r2 == 0 && bytes == 123 * 256, d);

    /* A three-page note: allocate, write, read back after a fresh osPfsInitPak, delete. */
    uint8_t name[16] = {0x1D, 0x1E, 0x1F}, ext[4] = {0}, out[768], in[768];
    int32_t file = -1, after_alloc = -1, after_delete = -1;
    for (int i = 0; i < 768; i++)
        out[i] = (uint8_t)(i * 7 + 3);
    int32_t ra = osPfsAllocateFile(pfs, 0x3459, 0x4E525545, name, ext, 768, &file);
    osPfsFreeBlocks(pfs, &after_alloc);
    int32_t rw = osPfsReadWriteFile(pfs, file, 1, 0, 768, out);
    int32_t ri = osPfsInitPak(si, pfs, 0);
    int32_t rr = osPfsReadWriteFile(pfs, file, 0, 0, 768, in);
    int32_t rd = osPfsDeleteFile(pfs, 0x3459, 0x4E525545, name, ext);
    osPfsFreeBlocks(pfs, &after_delete);
    snprintf(d, sizeof d, "alloc %d (file %d, %d free), write %d, init %d, read %d (%s), delete %d (%d free)", (int)ra,
             (int)file, (int)after_alloc, (int)rw, (int)ri, (int)rr, memcmp(in, out, 768) ? "differs" : "same",
             (int)rd, (int)after_delete);
    check("pak note round trip", ra == 0 && after_alloc == 120 * 256 && rw == 0 && ri == 0 && rr == 0 &&
                                     !memcmp(in, out, 768) && rd == 0 && after_delete == 123 * 256, d);
}

/* The main thread runs RSP tasks and answers with SP (and DP) events; the AI event comes when the output
 * queue is low. No graphics task: that needs a GL context (port/gfx has its own tests). */
static void t_rcp(void)
{
    void *q = queue(10, 8);
    OSMesg m[4] = {0};
    PcOSTask task = {0};
    char d[96];
    osSetEventMesg(OS_EVENT_SP, q, (OSMesg)4);
    osSetEventMesg(OS_EVENT_DP, q, (OSMesg)9);
    task.type = M_AUDTASK;   /* no SFDs: port/audio renders nothing */
    uint32_t before = pc_rcp_audio_tasks;
    osSpTaskLoad(&task);
    osSpTaskStartGo(&task);
    osRecvMesg(q, &m[0], OS_MESG_BLOCK);
    task.type = 7;           /* some other microcode: just done */
    osSpTaskStartGo(&task);
    osRecvMesg(q, &m[1], OS_MESG_BLOCK);
    osDpSetNextBuffer(NULL, 0);
    osRecvMesg(q, &m[2], OS_MESG_BLOCK);
    snprintf(d, sizeof d, "events %d %d %d, audio tasks %u", (int)(intptr_t)m[0], (int)(intptr_t)m[1],
             (int)(intptr_t)m[2], pc_rcp_audio_tasks - before);
    check("RSP tasks and DP buffers", m[0] == (OSMesg)4 && m[1] == (OSMesg)4 && m[2] == (OSMesg)9 &&
                                          pc_rcp_audio_tasks - before == 1, d);
    osSetEventMesg(OS_EVENT_SP, NULL, NULL);
    osSetEventMesg(OS_EVENT_DP, NULL, NULL);

    static int16_t buffer[192 * 2];
    int32_t rate = osAiSetFrequency(22050);
    osSetEventMesg(OS_EVENT_AI, q, (OSMesg)6);
    uint64_t t0 = osGetTime();
    osAiSetNextBuffer(buffer, sizeof buffer);   /* 192 frames queued: below 384, so an event */
    int r = osRecvMesg(q, &m[3], OS_MESG_BLOCK);
    double ms = (osGetTime() - t0) * 1000.0 / PC_COUNT_HZ;
    osSetEventMesg(OS_EVENT_AI, NULL, NULL);
    snprintf(d, sizeof d, "rate %d, event %d after %.1f ms", (int)rate, (int)(intptr_t)m[3], ms);
    check("AI frequency and event", rate == 22047 && r == 0 && m[3] == (OSMesg)6 && ms < SLACK_MS, d);
}

static void controller(void *arg)
{
    (void)arg;
    t_priorities();
    t_queue();
    t_blocking_send();
    t_yield();
    t_setpri();
    t_stop();
    t_events();
    t_timers();
    t_spin();
    t_memory();
    t_rcp();
    t_pif();
    if (pc_time_virtual()) {
        char d[64];
        snprintf(d, sizeof d, "%.0f ms of N64 time in %.0f ms", pc_host_ns() / 1e6, pc_real_ns() / 1e6);
        check("--fast: the virtual clock", pc_real_ns() < pc_host_ns(), d);
    }
    fprintf(stderr, "selftest: %d of %d passed\n", g_run - g_failed, g_run);
    pc_quit = g_failed ? 2 : 1;
    pc_os_wake_main();
    for (;;)
        osRecvMesg(queue(9, 1), NULL, OS_MESG_BLOCK);
}

/* As the game's idle_thread_entry: start the next thread, then idle. */
static void idle(void *arg)
{
    (void)arg;
    osCreateThread(g_thread[31], 2, controller, NULL, NULL, 50);
    osStartThread(g_thread[31]);
    for (;;)
        pc_os_idle();
}

static void boot(void *arg)
{
    (void)arg;
    osCreateThread(g_thread[30], 1, idle, NULL, NULL, 0);
    osStartThread(g_thread[30]);   /* the boot context ends here, as the game's main does */
}

int pc_selftest(void)
{
    PcRunOptions run = {.seconds = 30};
    pc_os_initialize();                /* the low-memory globals */
    pc_si_init(NULL, PC_PAK_MEMORY);   /* a formatted pak in memory */
    pc_os_boot(boot, NULL, "boot");
    int code = pc_ultra_run(&run);
    if (!pc_quit) {
        fprintf(stderr, "selftest: timed out\n");
        pc_os_dump_threads();
        code = 1;
    }
    fflush(NULL);
    _exit(code);
}
