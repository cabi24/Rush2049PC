/* port/ultra/timer.c: N64 time and the OS timers (docs/port/ULTRA.md).
 *
 * osGetTime counts at 46.875 MHz from the host's monotonic clock. osSetTimer timers live in a side table keyed
 * by the game's OSTimer (never written: it is N64-sized in the BSS arenas), and the main loop fires them
 * (pc_timer_service) by posting their message, as libultra's timer interrupt does.
 *
 * With --fast the clock is virtual: it stands still while an N64 thread computes and jumps to the main loop's next
 * deadline (a retrace, a timer, the AI) when every thread waits (sched.c), so the game runs as fast as the host
 * can, and a run is the same from one host to the next. */
#define _GNU_SOURCE
#include <pthread.h>
#include <time.h>

#include "ultra.h"

#define MAX_TIMERS 64

typedef struct {
    const void *key;   /* the game's OSTimer */
    uint64_t fire, interval;
    void *mq;
    OSMesg msg;
} PcTimer;

static pthread_mutex_t g_tlock = PTHREAD_MUTEX_INITIALIZER;
static PcTimer g_timers[MAX_TIMERS];
static int g_ntimers;
static int64_t g_time_offset;   /* osSetTime */
static int g_virtual;
static uint64_t g_virtual_ns = 1;   /* --fast: the clock, which only the main thread moves */

uint64_t pc_real_ns(void)
{
    static uint64_t start;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ns = (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
    if (!start)
        start = ns - 1;
    return ns - start;
}

uint64_t pc_host_ns(void)
{
    return g_virtual ? __atomic_load_n(&g_virtual_ns, __ATOMIC_ACQUIRE) : pc_real_ns();
}

void pc_time_set_virtual(int on) { g_virtual = on; }
int pc_time_virtual(void) { return g_virtual; }

void pc_time_advance(uint64_t ns)
{
    if (ns > __atomic_load_n(&g_virtual_ns, __ATOMIC_ACQUIRE))
        __atomic_store_n(&g_virtual_ns, ns, __ATOMIC_RELEASE);
}

uint64_t pc_ticks_now(void)
{
    return pc_host_ns() * 3 / 64 + (uint64_t)g_time_offset;   /* 46.875 MHz = 3/64 per ns */
}

uint64_t pc_ticks_to_ns(uint64_t ticks)
{
    return ticks * 64 / 3;
}

uint64_t osGetTime(void) { return pc_ticks_now(); }
uint32_t osGetCount(void) { return (uint32_t)pc_ticks_now(); }

void osSetTime(uint64_t t)
{
    g_time_offset += (int64_t)(t - pc_ticks_now());
}

static int find(const void *key)
{
    for (int i = 0; i < g_ntimers; i++)
        if (g_timers[i].key == key)
            return i;
    return -1;
}

int32_t osSetTimer(void *timer, uint64_t countdown, uint64_t interval, void *mq, OSMesg msg)
{
    pthread_mutex_lock(&g_tlock);
    int i = find(timer);
    if (i < 0) {
        if (g_ntimers == MAX_TIMERS)
            pc_fatal("more than %d OS timers", MAX_TIMERS);
        i = g_ntimers++;
    }
    g_timers[i] = (PcTimer){timer, pc_ticks_now() + (countdown ? countdown : interval), interval, mq, msg};
    pthread_mutex_unlock(&g_tlock);
    pc_os_wake_main();
    return 0;
}

int32_t osStopTimer(void *timer)
{
    pthread_mutex_lock(&g_tlock);
    int i = find(timer);
    if (i >= 0)
        g_timers[i] = g_timers[--g_ntimers];
    pthread_mutex_unlock(&g_tlock);
    return i >= 0 ? 0 : -1;
}

uint64_t pc_timer_service(uint64_t now)
{
    struct { void *mq; OSMesg msg; } due[MAX_TIMERS];
    int n = 0;
    uint64_t next = 0;
    pthread_mutex_lock(&g_tlock);
    for (int i = 0; i < g_ntimers;) {
        PcTimer *t = &g_timers[i];
        if (t->fire <= now) {
            due[n].mq = t->mq;
            due[n++].msg = t->msg;
            if (t->interval) {
                t->fire += t->interval;
                if (t->fire <= now)
                    t->fire = now + t->interval;
            } else {
                *t = g_timers[--g_ntimers];
                continue;
            }
        }
        if (!next || t->fire < next)
            next = t->fire;
        i++;
    }
    pthread_mutex_unlock(&g_tlock);
    for (int i = 0; i < n; i++)
        if (due[i].mq)
            pc_os_post(due[i].mq, due[i].msg);
    return next;
}
