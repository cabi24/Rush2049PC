/* port/ultra/rcp.c: RSP tasks, the RDP and the audio interface (docs/port/ULTRA.md).
 *
 * osSpTaskStartGo queues a copy of the task for the host main thread, which owns the GL context: graphics
 * tasks go to pc_gfx_run_dl and are followed by the SP and DP done events, audio tasks to pc_audio_run_task
 * and the SP event. Tasks run to completion, so osSpTaskYield never yields. The AI keeps libultra's two-entry
 * buffer FIFO on the host clock: a buffer plays for its length at the set frequency, and the AI event is sent
 * whenever one starts, as the hardware interrupt is raised when the next buffer moves into the DMA. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "ultra.h"

#define MAX_TASKS 8
#define VI_CLOCK_NTSC 48681812u

static pthread_mutex_t g_rlock = PTHREAD_MUTEX_INITIALIZER;
static PcOSTask g_tasks[MAX_TASKS];
static int g_ntasks, g_dp_pending;
uint32_t pc_rcp_gfx_tasks, pc_rcp_audio_tasks;
int pc_rcp_stop_at_gfx;

static struct {
    uint32_t freq;
    int n;                       /* buffers in the FIFO; q[0] plays */
    struct { const void *buf; uint32_t len; } q[2];
    uint64_t end_ns;             /* when q[0] finishes */
    int events;                  /* AI events to send */
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
        fflush(NULL);
        exit(0);
    }
}

/* ---------------------------------------------------------------- AI */

int32_t osAiSetFrequency(uint32_t frequency)
{
    if (!frequency)
        return -1;
    uint32_t dac = (2 * VI_CLOCK_NTSC / frequency + 1) / 2;   /* libultra's rounding */
    if (dac < 132)
        return -1;
    uint32_t actual = VI_CLOCK_NTSC / dac;
    pthread_mutex_lock(&g_rlock);
    g_ai.freq = actual;
    pthread_mutex_unlock(&g_rlock);
    pc_audio_set_frequency(actual);
    return (int32_t)actual;
}

static uint64_t buffer_ns(uint32_t len)
{
    uint32_t hz = g_ai.freq ? g_ai.freq : 32000;
    return (uint64_t)(len / 4) * 1000000000u / hz;
}

int32_t osAiSetNextBuffer(void *buf, uint32_t len)
{
    pthread_mutex_lock(&g_rlock);
    if (g_ai.n == 2) {
        pthread_mutex_unlock(&g_rlock);
        return -1;
    }
    g_ai.q[g_ai.n].buf = buf;
    g_ai.q[g_ai.n].len = len;
    if (g_ai.n++ == 0) {
        g_ai.end_ns = pc_host_ns() + buffer_ns(len);
        g_ai.events++;
    }
    pthread_mutex_unlock(&g_rlock);
    pc_audio_push(buf, len / 4);
    pc_os_wake_main();
    return 0;
}

uint32_t osAiGetLength(void)
{
    pthread_mutex_lock(&g_rlock);
    uint32_t left = 0;
    if (g_ai.n) {
        uint64_t now = pc_host_ns(), total = buffer_ns(g_ai.q[0].len);
        if (now < g_ai.end_ns && total)
            left = (uint32_t)((uint64_t)g_ai.q[0].len * (g_ai.end_ns - now) / total) & ~3u;
    }
    pthread_mutex_unlock(&g_rlock);
    return left;
}

uint32_t osAiGetStatus(void)
{
    pthread_mutex_lock(&g_rlock);
    uint32_t s = (g_ai.n == 2 ? 0x80000000u : 0) | (g_ai.n ? 0x40000000u : 0);   /* FIFO full, busy */
    pthread_mutex_unlock(&g_rlock);
    return s;
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
    while (g_ai.n && now >= g_ai.end_ns) {
        g_ai.q[0] = g_ai.q[1];
        if (--g_ai.n) {
            g_ai.end_ns += buffer_ns(g_ai.q[0].len);
            if (g_ai.end_ns < now)
                g_ai.end_ns = now + buffer_ns(g_ai.q[0].len);
            g_ai.events++;
        }
    }
    int ai = g_ai.events;
    g_ai.events = 0;
    if (g_ai.n && (!*next || g_ai.end_ns < *next))
        *next = g_ai.end_ns;
    pthread_mutex_unlock(&g_rlock);

    for (int i = 0; i < ai; i++)
        pc_os_event(OS_EVENT_AI);
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
