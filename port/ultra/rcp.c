/* port/ultra/rcp.c: RSP tasks, the RDP and the audio interface (docs/port/ULTRA.md).
 *
 * osSpTaskStartGo queues a copy of a graphics task for the host main thread, which owns the GL context: it goes
 * to pc_gfx_run_dl and is followed by the SP and DP done events. An audio task runs at once, in osSpTaskStartGo
 * (pc_audio_run_task), and sends the SP event. Tasks run to completion, so osSpTaskYield never yields.
 *
 * The AI hands each buffer to port/audio's output queue (pc_audio_push) and sends AI events while the queue and
 * the events not yet answered come to less than pc_audio_target_frames() (384 frames, two of the game's 192-frame
 * buffers; more with a device): the game's audio thread answers each event with one buffer (docs/port/AUDIO.md).
 * Up to 3 events are outstanding (the game's queue holds 4). The main loop checks, and in real time so does the
 * device's thread after each pull, so a main thread waiting for a swap doesn't starve the device. While a push
 * leaves the queue empty (no output), events are half a buffer apart, so the audio thread can't run flat out. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ultra.h"

#define MAX_TASKS 8
#define VI_CLOCK_NTSC 48681812u

static pthread_mutex_t g_rlock = PTHREAD_MUTEX_INITIALIZER;
static PcOSTask g_tasks[MAX_TASKS];
static int g_ntasks, g_dp_pending;
uint32_t pc_rcp_gfx_tasks, pc_rcp_audio_tasks;
int pc_rcp_stop_at_gfx;

#define AI_BUFFER_FRAMES 192
#define AI_MAX_OUTSTANDING 3     /* the game's AI queue has 4 slots (func_80010C68) */
#define AI_TIMEOUT_NS 50000000u  /* an event not answered in 50 ms may have been dropped */

static struct {
    uint32_t freq;
    int started;         /* the game has queued its first buffer */
    int outstanding;     /* events sent and not answered by a buffer yet */
    int landed;          /* the last buffer reached the queue (an output is there) */
    int hooked;          /* the pull hook is set */
    uint64_t sent_ns;
} g_ai;

/* ---------------------------------------------------------------- RSP and RDP */

void osSpTaskLoad(void *task) { (void)task; }

void osSpTaskStartGo(void *task)
{
    const PcOSTask *t = task;
    if (t->type == M_AUDTASK) {
        /* The RSP runs audio tasks at once, on the thread that starts them (the game's scheduler): behind the main
         * thread's graphics and swaps they would be late, and the AI would push ring buffers they hadn't rendered
         * yet. The scheduler takes the SP event from its queue after this returns. */
        pc_rcp_audio_tasks++;
        pc_audio_run_task(t->data_ptr, t->data_size);
        pc_os_event(OS_EVENT_SP);
        return;
    }
    pthread_mutex_lock(&g_rlock);
    if (g_ntasks == MAX_TASKS)
        pc_fatal("RSP task queue full");
    g_tasks[g_ntasks++] = *(const PcOSTask *)task;
    pthread_mutex_unlock(&g_rlock);
    pc_os_wake_main();
}

void osSpTaskYield(void) {}
uint32_t osSpTaskYielded(void *task) { (void)task; return 0; }

int32_t osDpSetNextBuffer(void *buf, uint64_t size)
{
    (void)buf, (void)size;
    pthread_mutex_lock(&g_rlock);
    g_dp_pending++;
    pthread_mutex_unlock(&g_rlock);
    pc_os_wake_main();
    return 0;
}

void osDpGetCounters(uint32_t *counters)
{
    for (int i = 0; i < 4; i++)
        counters[i] = 0;
}

int32_t osDpIsBusy(void) { return 0; }

static void report_first_gfx(const PcOSTask *t)
{
    fprintf(stderr, "rush2049pc: first graphics task at frame %u (%.2f s): data_ptr %p, %u bytes, ucode %p, "
            "framebuffer swaps follow\n", pc_frame, pc_host_ns() / 1e9, (void *)t->data_ptr, t->data_size,
            (void *)t->ucode);
    if (pc_rcp_stop_at_gfx) {
        pc_os_dump_threads();
        fprintf(stderr, "rush2049pc: --stop-at-gfx: stopping here\n");
        pc_si_flush();
        fflush(NULL);
        if (pc_on_exit)
            pc_on_exit(0);
        _exit(0);   /* the N64 threads are still running game code: no atexit handlers */
    }
}

/* ---------------------------------------------------------------- AI */

int32_t osAiSetFrequency(uint32_t frequency)
{
    if (!frequency)
        return -1;
    uint32_t dac = (uint32_t)((float)VI_CLOCK_NTSC / (float)frequency + 0.5f);   /* as libultra rounds it */
    if (dac < 132)
        return -1;
    uint32_t actual = VI_CLOCK_NTSC / dac;
    pthread_mutex_lock(&g_rlock);
    g_ai.freq = actual;
    pthread_mutex_unlock(&g_rlock);
    pc_audio_set_frequency(actual);
    return (int32_t)actual;
}

static uint64_t frames_ns(uint64_t frames)
{
    return frames * 1000000000u / (g_ai.freq ? g_ai.freq : 22050);
}

static void ai_pulled(void);

int32_t osAiSetNextBuffer(void *buf, uint32_t len)
{
    pc_audio_push(buf, len / 4);
    size_t queued = pc_audio_queued_frames();
    pthread_mutex_lock(&g_rlock);
    g_ai.started = 1;
    g_ai.landed = queued >= len / 8;
    if (g_ai.outstanding)
        g_ai.outstanding--;
    int hook = !g_ai.hooked;
    g_ai.hooked = 1;
    pthread_mutex_unlock(&g_rlock);
    if (hook)
        pc_audio_set_pull_hook(ai_pulled);
    pc_os_wake_main();
    return 0;
}

uint32_t osAiGetLength(void) { return (uint32_t)pc_audio_queued_frames() * 4; }

uint32_t osAiGetStatus(void)
{
    return pc_audio_queued_frames() ? 0x40000000u : 0;   /* busy */
}

/* With g_rlock: how many AI events to send now; *again gets when to look next (0: when a buffer comes). */
static int ai_due(uint64_t now, uint64_t *again)
{
    *again = 0;
    if (!g_ai.started)
        return 0;
    if (now < g_ai.sent_ns)   /* the other thread sent since this one read the clock */
        now = g_ai.sent_ns;
    if (g_ai.outstanding && now - g_ai.sent_ns > AI_TIMEOUT_NS)
        g_ai.outstanding = 0;
    size_t queued = pc_audio_queued_frames(), target = pc_audio_target_frames();
    size_t have = queued + (size_t)g_ai.outstanding * AI_BUFFER_FRAMES;
    if (have >= target) {
        uint64_t ahead = frames_ns(have - target);
        *again = now + (ahead > 1000000u ? ahead : 1000000u);
        return 0;
    }
    if (g_ai.outstanding >= AI_MAX_OUTSTANDING) {
        *again = g_ai.sent_ns + AI_TIMEOUT_NS;
        return 0;
    }
    uint64_t gap = frames_ns(AI_BUFFER_FRAMES / 2);
    if (!g_ai.landed && now - g_ai.sent_ns < gap) {
        *again = g_ai.sent_ns + gap;
        return 0;
    }
    int n = 0;
    do {
        n++;
        g_ai.outstanding++;
        have += AI_BUFFER_FRAMES;
    } while (have < target && g_ai.outstanding < AI_MAX_OUTSTANDING && g_ai.landed);
    g_ai.sent_ns = now;
    *again = now + AI_TIMEOUT_NS;
    return n;
}

/* Main thread: the AI events due; *next gets when to look again. */
static int ai_service(uint64_t now, uint64_t *next)
{
    uint64_t again;
    pthread_mutex_lock(&g_rlock);
    int n = ai_due(now, &again);
    pthread_mutex_unlock(&g_rlock);
    if (again && (!*next || again < *next))
        *next = again;
    for (int i = 0; i < n; i++)
        pc_os_event(OS_EVENT_AI);
    return n;
}

/* The device's thread, after each pull (pc_audio_set_pull_hook). Not with --fast: there the main loop alone
 * sends events, at moments the virtual clock fixes. */
static void ai_pulled(void)
{
    uint64_t again;
    if (pc_time_virtual())
        return;
    pthread_mutex_lock(&g_rlock);
    int n = ai_due(pc_host_ns(), &again);
    pthread_mutex_unlock(&g_rlock);
    for (int i = 0; i < n; i++)
        pc_os_event(OS_EVENT_AI);
}

/* ---------------------------------------------------------------- main thread */

int pc_rcp_pending(void)
{
    pthread_mutex_lock(&g_rlock);
    int n = g_ntasks;
    pthread_mutex_unlock(&g_rlock);
    return n;
}

int pc_rcp_service(uint64_t now, uint64_t *next)
{
    PcOSTask tasks[MAX_TASKS];
    pthread_mutex_lock(&g_rlock);
    int n = g_ntasks, dp = g_dp_pending;
    for (int i = 0; i < n; i++)
        tasks[i] = g_tasks[i];
    g_ntasks = g_dp_pending = 0;
    pthread_mutex_unlock(&g_rlock);

    int ai = ai_service(now, next);
    for (int i = 0; i < n; i++) {
        const PcOSTask *t = &tasks[i];
        if (t->type == M_GFXTASK) {
            if (pc_rcp_gfx_tasks++ == 0)
                report_first_gfx(t);
            pc_gfx_run_dl(t->data_ptr);
            pc_os_event(OS_EVENT_SP);
            pc_os_event(OS_EVENT_DP);
        } else {
            pc_log("RSP task of type %u ignored", t->type);
            pc_os_event(OS_EVENT_SP);
        }
    }
    for (int i = 0; i < dp; i++)
        pc_os_event(OS_EVENT_DP);
    return n || dp || ai;
}
