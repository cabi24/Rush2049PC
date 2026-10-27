/* port/ultra/rcp.c: RSP tasks, the RDP and the audio interface (docs/port/ULTRA.md).
 *
 * osSpTaskStartGo queues a copy of the task for the host main thread, which owns the GL context: graphics
 * tasks go to pc_gfx_run_dl and are followed by the SP and DP done events, audio tasks to pc_audio_run_task
 * and the SP event. Tasks run to completion, so osSpTaskYield never yields. The AI hands each buffer to
 * port/audio's output queue (pc_audio_push) and sends the AI event when fewer than 384 frames (two of the
 * game's 192-frame buffers) are left in it: the game's audio thread answers each event with one buffer
 * (docs/port/AUDIO.md). One event is outstanding at a time, and events are at least half a buffer apart, so a
 * missing output can't make the audio thread run flat out. */
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

#define AI_LOW_FRAMES 384
#define AI_BUFFER_FRAMES 192

static struct {
    uint32_t freq;
    int started;         /* the game has queued its first buffer */
    int waiting;         /* an AI event is out and not answered yet */
    uint64_t sent_ns;
} g_ai;

/* ---------------------------------------------------------------- RSP and RDP */

void osSpTaskLoad(void *task) { (void)task; }

void osSpTaskStartGo(void *task)
{
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

int32_t osAiSetNextBuffer(void *buf, uint32_t len)
{
    pc_audio_push(buf, len / 4);
    pthread_mutex_lock(&g_rlock);
    g_ai.started = 1;
    g_ai.waiting = 0;
    pthread_mutex_unlock(&g_rlock);
    pc_os_wake_main();
    return 0;
}

uint32_t osAiGetLength(void) { return (uint32_t)pc_audio_queued_frames() * 4; }

uint32_t osAiGetStatus(void)
{
    return pc_audio_queued_frames() ? 0x40000000u : 0;   /* busy */
}

/* Main thread: send the AI event when the output queue runs low; *next gets when to look again. */
static int ai_service(uint64_t now, uint64_t *next)
{
    pthread_mutex_lock(&g_rlock);
    int send = 0;
    uint64_t again = 0, gap = frames_ns(AI_BUFFER_FRAMES / 2);
    if (g_ai.waiting && now - g_ai.sent_ns > 50000000u)
        g_ai.waiting = 0;   /* not answered in 50 ms: the event may have been dropped */
    if (g_ai.started && !g_ai.waiting) {
        size_t queued = pc_audio_queued_frames();
        if (queued < AI_LOW_FRAMES && now - g_ai.sent_ns >= gap) {
            send = 1;
            g_ai.waiting = 1;
            g_ai.sent_ns = now;
        } else if (queued < AI_LOW_FRAMES) {
            again = g_ai.sent_ns + gap;
        } else {
            uint64_t ahead = frames_ns(queued - AI_LOW_FRAMES);
            again = now + (ahead > 1000000u ? ahead : 1000000u);
        }
    } else if (g_ai.waiting) {
        again = g_ai.sent_ns + 50000000u;
    }
    pthread_mutex_unlock(&g_rlock);
    if (again && (!*next || again < *next))
        *next = again;
    if (send)
        pc_os_event(OS_EVENT_AI);
    return send;
}

/* ---------------------------------------------------------------- main thread */

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
            if (t->type == M_AUDTASK) {
                pc_rcp_audio_tasks++;
                pc_audio_run_task(t->data_ptr, t->data_size);
            } else {
                pc_log("RSP task of type %u ignored", t->type);
            }
            pc_os_event(OS_EVENT_SP);
        }
    }
    for (int i = 0; i < dp; i++)
        pc_os_event(OS_EVENT_DP);
    return n || dp || ai;
}
