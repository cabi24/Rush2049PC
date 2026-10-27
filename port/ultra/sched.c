/* port/ultra/sched.c: N64 threads, message queues and events on host threads (docs/port/ULTRA.md).
 *
 * Each OSThread the game creates gets a host thread, and one of them runs at a time: the baton (g_running).
 * The baton moves only inside libultra calls (a blocking osRecvMesg/osSendMesg/osJamMesg, osStartThread,
 * osStopThread, osSetThreadPri, osYieldThread, osDestroyThread) with libultra's rules: the highest-priority
 * runnable thread runs, equal priorities in the order they became runnable, and a call that wakes a
 * higher-priority thread hands it the baton at once. Device and timer events come from the host main thread
 * (pc_os_post): they make threads runnable and hand the baton over when no thread holds it or the holder
 * waits for events (idle, busy-wait yields), but they never take it from a thread that is computing, as an
 * N64 interrupt would. That is why the game's busy-waits yield (pc_os_spin_yield).
 *
 * State lives in side tables keyed by the game's object addresses. Nothing is written into the game's
 * OSThread or OSMesgQueue: they are N64-sized in the BSS arenas, and the native structs are larger. Message
 * buffers are host-side for the same reason (OSMesg slots are 8 bytes here, 4 in the game's arrays).
 *
 * With --fast (timer.c's virtual clock) the main thread moves the clock to its next deadline only when the
 * system is idle: no thread holds the baton, or its holder waits for events with nothing else runnable. Each
 * change to idle wakes the main thread for that. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "ultra.h"

enum { TS_STOPPED = 1, TS_RUNNABLE = 2, TS_RUNNING = 4, TS_WAITING = 8, TS_DEAD = 16 };
#define MAX_THREADS 64
#define BOOT_PRI 256   /* above OS_PRIORITY_MAX: nothing preempts the boot context */
#define STACK_SIZE (2u << 20)

typedef struct PcQueue {
    const void *key;           /* the game's OSMesgQueue */
    OSMesg *msg;
    int32_t count, valid, first;
    struct PcQueue *next;
} PcQueue;

typedef struct PcThread {
    const void *key;           /* the game's OSThread (NULL for the boot context and orphans) */
    int32_t id, pri;
    int state;
    uint64_t order;            /* FIFO among equal priorities: when it last became runnable or blocked */
    void (*entry)(void *);
    void *arg;
    int started, boot;
    pthread_t host;
    pthread_cond_t cv;
    PcQueue *wait_q;           /* blocked in recv (wait_send 0) or send (1) on this queue */
    int wait_send;
    int event_wait;            /* holds the baton but waits for an event (idle, spin yield) */
    uint64_t nap_until;        /* --fast: a busy-wait's nap ends at this time of the virtual clock */
    uint32_t spins;            /* consecutive busy-wait yields with nothing to run */
    char name[24];
} PcThread;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_main_cv;
static int g_main_wake;
static PcThread g_threads[MAX_THREADS];
static int g_nthreads;
static PcThread *g_running;
static uint64_t g_order;
static _Thread_local PcThread *tls_self;
#define QHASH 256
static PcQueue *g_queues[QHASH];
static struct { const void *mq; OSMesg msg; } g_events[OS_NUM_EVENTS];
int pc_os_trace;

const char *pc_os_thread_name(void) { return tls_self ? tls_self->name : "main"; }

static void init_cv(pthread_cond_t *cv)
{
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(cv, &a);
    pthread_condattr_destroy(&a);
}

#ifdef _WIN32
/* Windows' timeouts run on the system timer, which some machines keep at 15.6 ms whatever timeBeginPeriod asks for
 * (the windows-latest runner did: 200 one-millisecond busy-wait naps took 3.1 s). So the two timed waits use a
 * high-resolution waitable timer (Windows 10 1803 and later; a plain one before) instead of a condition
 * variable's timeout: a busy-wait naps on it with the lock released, and the main thread waits for it or for its
 * wake-up event (docs/port/WINDOWS.md). */
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2
#endif
static HANDLE g_main_event;

/* This thread's timer, armed to fire ns from now. */
static HANDLE hires_timer(uint64_t ns)
{
    static _Thread_local HANDLE timer;
    LARGE_INTEGER due;
    if (!timer)
        timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer)
        timer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    due.QuadPart = -(LONGLONG)((ns + 99) / 100);   /* relative, in 100 ns units */
    SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
    return timer;
}

/* A busy-wait's nap: ns with the lock released. An event that comes meanwhile is seen when it ends. */
static void nap(pthread_cond_t *cv, uint64_t ns_from_now)
{
    (void)cv;
    pthread_mutex_unlock(&g_lock);
    WaitForSingleObject(hires_timer(ns_from_now), INFINITE);
    pthread_mutex_lock(&g_lock);
}
#else
static void deadline_ts(struct timespec *ts, uint64_t ns_from_now)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
    ts->tv_sec += (time_t)(ns_from_now / 1000000000u);
    ts->tv_nsec += (long)(ns_from_now % 1000000000u);
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

/* Up to ns, or until cv is signalled: a busy-wait's nap, and the main thread's wait. */
static void nap(pthread_cond_t *cv, uint64_t ns_from_now)
{
    struct timespec ts;
    deadline_ts(&ts, ns_from_now);
    pthread_cond_timedwait(cv, &g_lock, &ts);
}
#endif

/* ---------------------------------------------------------------- side tables */

static PcThread *thread_of(const void *key)
{
    for (int i = 0; i < g_nthreads; i++)
        if (g_threads[i].key == key && key)
            return &g_threads[i];
    return NULL;
}

static PcThread *thread_new(void)
{
    for (int i = 0; i < g_nthreads; i++)   /* reuse slots that never got a host thread */
        if (!g_threads[i].key && !g_threads[i].started && !g_threads[i].boot)
            return &g_threads[i];
    if (g_nthreads == MAX_THREADS)
        pc_fatal("more than %d N64 threads", MAX_THREADS);
    PcThread *t = &g_threads[g_nthreads++];
    init_cv(&t->cv);
    return t;
}

static unsigned qhash(const void *key)
{
    return (unsigned)((((uintptr_t)key >> 3) * 2654435761u) >> 8) & (QHASH - 1);
}

static PcQueue *queue_of(const void *key, int create)
{
    PcQueue **b = &g_queues[qhash(key)];
    for (PcQueue *q = *b; q; q = q->next)
        if (q->key == key)
            return q;
    if (!create)
        return NULL;
    PcQueue *q = calloc(1, sizeof *q);
    q->key = key;
    q->next = *b;
    *b = q;
    return q;
}

/* A queue the game uses before creating it behaves as a zero-capacity one, as a zeroed OSMesgQueue does. */
static PcQueue *queue_used(const void *key)
{
    PcQueue *q = queue_of(key, 0);
    if (!q) {
        pc_log("message queue %p used before osCreateMesgQueue (thread %s)", key, pc_os_thread_name());
        q = queue_of(key, 1);
    }
    return q;
}

/* ---------------------------------------------------------------- the baton (lock held) */

static PcThread *best(const PcThread *except);

/* --fast: nothing will happen until the main thread sends an event, so it may move the clock on. The holder of
 * the baton waits for events and no thread that would take it over is runnable (a lower-priority one runs only
 * after a busy-wait's 200 naps). */
static int system_idle(void)
{
    if (!g_running)
        return 1;
    PcThread *b = best(g_running);
    return g_running->event_wait && !(b && b->pri >= g_running->pri);
}

static void main_signal(void);

/* --fast: the system may have become idle; the main thread looks again. */
static void idle_changed(void)
{
    if (pc_time_virtual())
        main_signal();
}

static PcThread *best(const PcThread *except)
{
    PcThread *b = NULL;
    for (int i = 0; i < g_nthreads; i++) {
        PcThread *t = &g_threads[i];
        if (t == except || t->state != TS_RUNNABLE)
            continue;
        if (!b || t->pri > b->pri || (t->pri == b->pri && t->order < b->order))
            b = t;
    }
    return b;
}

static void give(PcThread *t)
{
    g_running = t;
    t->state = TS_RUNNING;
    pthread_cond_signal(&t->cv);
}

/* A thread became runnable: run it if no thread holds the baton, or wake a holder that waits for events. */
static void kick(void)
{
    if (!g_running) {
        PcThread *b = best(NULL);
        if (b)
            give(b);
    } else if (g_running->event_wait) {
        pthread_cond_signal(&g_running->cv);
    }
}

static void make_runnable(PcThread *t)
{
    t->state = TS_RUNNABLE;
    t->order = ++g_order;
    t->wait_q = NULL;
    kick();
}

/* Hand the baton to the best runnable thread (self included, if self is runnable) and wait until self gets
 * it back. The caller has set self's state. A dead thread waits forever: its host thread is parked. */
static void switch_from(PcThread *self)
{
    PcThread *next = best(NULL);
    if (next == self) {
        self->state = TS_RUNNING;
        return;
    }
    if (pc_os_trace)
        pc_log("switch %s -> %s", self->name, next ? next->name : "(none)");
    self->spins = 0;
    g_running = NULL;
    if (next)
        give(next);
    else
        idle_changed();
    while (g_running != self)
        pthread_cond_wait(&self->cv, &g_lock);
}

/* Libultra's preemption on a wake-up: a strictly higher-priority runnable thread takes the baton. */
static void preempt(PcThread *self)
{
    PcThread *b = best(self);
    if (self && b && b->pri > self->pri) {
        self->state = TS_RUNNABLE;
        self->order = ++g_order;
        switch_from(self);
    }
}

static PcThread *self_or_die(const char *call)
{
    if (!tls_self)
        pc_fatal("%s called from the host main thread", call);
    return tls_self;
}

static void *thread_main(void *p)
{
    PcThread *self = p;
    tls_self = self;
    pc_fault_thread_init();
    pthread_mutex_lock(&g_lock);
    while (g_running != self)
        pthread_cond_wait(&self->cv, &g_lock);
    pthread_mutex_unlock(&g_lock);
    self->entry(self->arg);
    /* Returned: libultra's __osCleanupThread destroys the thread. */
    pthread_mutex_lock(&g_lock);
    if (pc_verbose)
        pc_log("%s returned", self->name);
    self->state = TS_DEAD;
    switch_from(self);
    return NULL;
}

static void start_host(PcThread *t)
{
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, STACK_SIZE);   /* the game's own stacks (gStack*) are not used */
    int err = pthread_create(&t->host, &a, thread_main, t);
    pthread_attr_destroy(&a);
    if (err)
        pc_fatal("pthread_create: %s", strerror(err));
    t->started = 1;
}

void pc_os_boot(void (*entry)(void *), void *arg, const char *name)
{
    init_cv(&g_main_cv);
#ifdef _WIN32
    g_main_event = CreateEventW(NULL, FALSE, FALSE, NULL);
#endif
    pthread_mutex_lock(&g_lock);
    PcThread *t = thread_new();
    t->boot = 1;
    t->pri = BOOT_PRI;
    t->entry = entry;
    t->arg = arg;
    snprintf(t->name, sizeof t->name, "%s", name);
    start_host(t);
    make_runnable(t);
    pthread_mutex_unlock(&g_lock);
}

/* ---------------------------------------------------------------- threads */

void osCreateThread(void *thread, int32_t id, void (*entry)(void *), void *arg, void *sp, int32_t pri)
{
    pthread_mutex_lock(&g_lock);
    PcThread *t = thread_of(thread);
    if (t && (t->started || t == tls_self)) {
        /* Created again while its host thread exists: that one is orphaned (it stays parked). */
        if (t->state != TS_DEAD && t->state != TS_STOPPED)
            pc_log("osCreateThread on live thread %s", t->name);
        t->key = NULL;
        t->state = TS_DEAD;
        t = NULL;
    }
    if (!t)
        t = thread_new();
    t->key = thread;
    t->id = id;
    t->pri = pri;
    t->entry = entry;
    t->arg = arg;
    t->state = TS_STOPPED;
    t->wait_q = NULL;
    snprintf(t->name, sizeof t->name, "thread %d", (int)id);
    if (pc_verbose)
        pc_log("osCreateThread %s (%p) priority %d", t->name, thread, (int)pri);
    pthread_mutex_unlock(&g_lock);
}

void osStartThread(void *thread)
{
    pthread_mutex_lock(&g_lock);
    PcThread *self = tls_self, *t = thread_of(thread);
    if (!t) {
        pc_log("osStartThread on unknown thread %p", thread);
    } else if (t->state == TS_STOPPED) {
        if (!t->started)
            start_host(t);
        make_runnable(t);   /* one stopped while blocked re-checks its queue, as in libultra */
    } else if (t->state == TS_WAITING) {
        make_runnable(t);
    }
    if (self && self->boot) {
        /* The boot context's first osStartThread dispatches and never returns (libultra's main). */
        self->state = TS_DEAD;
        switch_from(self);
    }
    preempt(self);
    pthread_mutex_unlock(&g_lock);
}

void osStopThread(void *thread)
{
    pthread_mutex_lock(&g_lock);
    PcThread *self = tls_self, *t = thread ? thread_of(thread) : self;
    if (t && t == self) {
        t->state = TS_STOPPED;
        switch_from(self);
    } else if (t && (t->state == TS_RUNNABLE || t->state == TS_WAITING)) {
        t->state = TS_STOPPED;   /* keeps wait_q: restarted, it re-checks */
    }
    pthread_mutex_unlock(&g_lock);
}

void osDestroyThread(void *thread)
{
    pthread_mutex_lock(&g_lock);
    PcThread *self = tls_self, *t = thread ? thread_of(thread) : self;
    if (t) {
        t->state = TS_DEAD;
        if (t == self)
            switch_from(self);   /* never returns */
        t->key = NULL;
    }
    pthread_mutex_unlock(&g_lock);
}

void osYieldThread(void)
{
    pthread_mutex_lock(&g_lock);
    PcThread *self = self_or_die("osYieldThread");
    self->state = TS_RUNNABLE;
    self->order = ++g_order;
    switch_from(self);
    pthread_mutex_unlock(&g_lock);
}

void osSetThreadPri(void *thread, int32_t pri)
{
    pthread_mutex_lock(&g_lock);
    PcThread *self = tls_self, *t = thread ? thread_of(thread) : self;
    if (t && t->pri != pri) {
        t->pri = pri;
        if (self)
            preempt(self);
    }
    pthread_mutex_unlock(&g_lock);
}

int32_t osGetThreadPri(void *thread)
{
    pthread_mutex_lock(&g_lock);
    PcThread *t = thread ? thread_of(thread) : tls_self;
    int32_t pri = t ? t->pri : 0;
    pthread_mutex_unlock(&g_lock);
    return pri;
}

int32_t osGetThreadId(void *thread)
{
    pthread_mutex_lock(&g_lock);
    PcThread *t = thread ? thread_of(thread) : tls_self;
    int32_t id = t ? t->id : 0;
    pthread_mutex_unlock(&g_lock);
    return id;
}

/* ---------------------------------------------------------------- message queues */

static void wake_one(PcQueue *q, int send)
{
    PcThread *b = NULL;
    for (int i = 0; i < g_nthreads; i++) {
        PcThread *t = &g_threads[i];
        if (t->state != TS_WAITING || t->wait_q != q || t->wait_send != send)
            continue;
        if (!b || t->pri > b->pri || (t->pri == b->pri && t->order < b->order))
            b = t;
    }
    if (b)
        make_runnable(b);
}

static void block_on(PcThread *self, PcQueue *q, int send)
{
    self->state = TS_WAITING;
    self->wait_q = q;
    self->wait_send = send;
    self->order = ++g_order;
    switch_from(self);
}

void osCreateMesgQueue(void *mq, OSMesg *msg, int32_t count)
{
    pthread_mutex_lock(&g_lock);
    PcQueue *q = queue_of(mq, 1);
    free(q->msg);
    q->count = count > 0 ? count : 0;
    q->msg = calloc(q->count ? (size_t)q->count : 1, sizeof *q->msg);
    q->valid = q->first = 0;
    pthread_mutex_unlock(&g_lock);
}

static int32_t send_locked(void *mq, OSMesg msg, int32_t flags, int jam, const char *call)
{
    PcThread *self = tls_self;
    PcQueue *q = queue_used(mq);
    while (q->valid >= q->count) {
        if (flags == OS_MESG_NOBLOCK || !self)
            return -1;
        if (pc_os_trace)
            pc_log("%s blocks in %s (queue %p full)", self->name, call, mq);
        block_on(self, q, 1);
    }
    if (jam) {
        q->first = (q->first + q->count - 1) % q->count;
        q->msg[q->first] = msg;
    } else {
        q->msg[(q->first + q->valid) % q->count] = msg;
    }
    q->valid++;
    wake_one(q, 0);
    if (self)
        preempt(self);
    return 0;
}

int32_t osSendMesg(void *mq, OSMesg msg, int32_t flags)
{
    pthread_mutex_lock(&g_lock);
    int32_t r = send_locked(mq, msg, flags, 0, "osSendMesg");
    pthread_mutex_unlock(&g_lock);
    return r;
}

int32_t osJamMesg(void *mq, OSMesg msg, int32_t flags)
{
    pthread_mutex_lock(&g_lock);
    int32_t r = send_locked(mq, msg, flags, 1, "osJamMesg");
    pthread_mutex_unlock(&g_lock);
    return r;
}

int32_t osRecvMesg(void *mq, OSMesg *msg, int32_t flags)
{
    pthread_mutex_lock(&g_lock);
    PcThread *self = tls_self;
    PcQueue *q = queue_used(mq);
    while (q->valid == 0) {
        if (flags == OS_MESG_NOBLOCK || !self) {
            pthread_mutex_unlock(&g_lock);
            return -1;
        }
        if (pc_os_trace)
            pc_log("%s blocks in osRecvMesg (queue %p)", self->name, mq);
        block_on(self, q, 0);
    }
    if (msg)
        *msg = q->msg[q->first];
    q->first = (q->first + 1) % q->count;
    q->valid--;
    wake_one(q, 1);
    if (self)
        preempt(self);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int pc_os_post(void *mq, OSMesg msg)
{
    pthread_mutex_lock(&g_lock);
    PcQueue *q = queue_of(mq, 0);
    int r = -1;
    if (q && q->valid < q->count) {
        q->msg[(q->first + q->valid) % q->count] = msg;
        q->valid++;
        wake_one(q, 0);
        r = 0;
    }
    pthread_mutex_unlock(&g_lock);
    return r;
}

/* ---------------------------------------------------------------- events */

void osSetEventMesg(uint32_t event, void *mq, OSMesg msg)
{
    pthread_mutex_lock(&g_lock);
    if (event < OS_NUM_EVENTS) {
        g_events[event].mq = mq;
        g_events[event].msg = msg;
    }
    pthread_mutex_unlock(&g_lock);
}

void pc_os_event(int event)
{
    pthread_mutex_lock(&g_lock);
    const void *mq = g_events[event].mq;
    OSMesg msg = g_events[event].msg;
    pthread_mutex_unlock(&g_lock);
    if (mq && pc_os_post((void *)mq, msg) < 0 && pc_verbose > 1)
        pc_log("event %d dropped: queue %p full", event, mq);
}

/* ---------------------------------------------------------------- waiting for events */

/* One of the game's busy-waits on memory another thread writes. A thread of equal or higher priority that
 * can run gets the baton; otherwise wait up to 1 ms for an event to make one runnable. After 200 ms with
 * nothing to run at our priority or above, a lower-priority thread runs once, so a busy-wait whose writer
 * the N64 would have run from an interrupt can't hang (logged once per site). */
void pc_os_spin_yield(const char *site)
{
    pthread_mutex_lock(&g_lock);
    PcThread *self = tls_self;
    if (!self) {
        pthread_mutex_unlock(&g_lock);
        sched_yield();
        return;
    }
    PcThread *b = best(self);
    if (!(b && b->pri >= self->pri)) {
        self->event_wait = 1;
        if (pc_time_virtual()) {
            /* 1 ms of the virtual clock: the main thread moves it on and wakes us (wait_main_virtual) */
            self->nap_until = pc_host_ns() + 1000000;
            idle_changed();
            while (pc_host_ns() < self->nap_until && !((b = best(self)) && b->pri >= self->pri))
                pthread_cond_wait(&self->cv, &g_lock);
            self->nap_until = 0;
        } else {
            nap(&self->cv, 1000000);
        }
        self->event_wait = 0;
        b = best(self);
    }
    if (b && b->pri >= self->pri) {
        self->state = TS_RUNNABLE;
        self->order = ++g_order;
        switch_from(self);
    } else if (b && ++self->spins >= 200) {
        static const char *logged[16];
        int seen = 0;
        for (int i = 0; i < 16 && logged[i]; i++)
            seen |= logged[i] == site;
        if (!seen) {
            for (int i = 0; i < 16; i++)
                if (!logged[i]) {
                    logged[i] = site;
                    break;
                }
            pc_log("busy-wait at %s (%s): running lower-priority %s", site, self->name, b->name);
        }
        self->state = TS_RUNNABLE;
        self->order = ++g_order;
        self->spins = 0;
        g_running = NULL;
        give(b);
        while (g_running != self)
            pthread_cond_wait(&self->cv, &g_lock);
    }
    pthread_mutex_unlock(&g_lock);
}

void pc_os_idle(void)
{
    pthread_mutex_lock(&g_lock);
    PcThread *self = self_or_die("pc_os_idle");
    for (;;) {
        PcThread *b = best(self);
        if (b) {
            self->state = TS_RUNNABLE;
            self->order = ++g_order;
            switch_from(self);
            break;
        }
        self->event_wait = 1;
        idle_changed();
        pthread_cond_wait(&self->cv, &g_lock);
        self->event_wait = 0;
    }
    pthread_mutex_unlock(&g_lock);
}

/* --fast: wait for a wake-up, or for the system to go idle and then move the clock to the deadline. At most
 * 100 ms of real time, so that the main loop sees a run that stopped moving the clock (a hang). */
static void wait_main_virtual(uint64_t deadline_ns)
{
    uint64_t until = pc_real_ns() + 100000000u;
    pthread_mutex_lock(&g_lock);
    while (!g_main_wake && pc_real_ns() < until) {
        if (system_idle()) {
            PcThread *t = g_running;
            if (t && t->nap_until && t->nap_until < deadline_ns)
                deadline_ns = t->nap_until;   /* a busy-wait's nap ends first */
            pc_time_advance(deadline_ns);
            if (t && t->nap_until)
                pthread_cond_signal(&t->cv);
            break;
        }
#ifdef _WIN32
        pthread_mutex_unlock(&g_lock);
        WaitForSingleObject(g_main_event, 1);
        pthread_mutex_lock(&g_lock);
#else
        nap(&g_main_cv, 1000000);
#endif
    }
    g_main_wake = 0;
    pthread_mutex_unlock(&g_lock);
}

#ifdef _WIN32
static void main_signal(void)
{
    if (g_main_event)
        SetEvent(g_main_event);
}

/* The main thread's wake-up is an auto-reset event, so a wake between the check and the wait isn't lost. */
void pc_os_wait_main(uint64_t deadline_ns)
{
    if (pc_time_virtual()) {
        wait_main_virtual(deadline_ns);
        return;
    }
    pthread_mutex_lock(&g_lock);
    uint64_t now = pc_host_ns();
    int wait = !g_main_wake && deadline_ns > now;
    pthread_mutex_unlock(&g_lock);
    if (wait) {
        HANDLE h[2] = {g_main_event, hires_timer(deadline_ns - now)};
        WaitForMultipleObjects(2, h, FALSE, INFINITE);
    }
    pthread_mutex_lock(&g_lock);
    g_main_wake = 0;
    pthread_mutex_unlock(&g_lock);
}

void pc_os_wake_main(void)
{
    pthread_mutex_lock(&g_lock);
    g_main_wake = 1;
    if (g_main_event)
        SetEvent(g_main_event);
    pthread_mutex_unlock(&g_lock);
}
#else
static void main_signal(void) { pthread_cond_signal(&g_main_cv); }

void pc_os_wait_main(uint64_t deadline_ns)
{
    if (pc_time_virtual()) {
        wait_main_virtual(deadline_ns);
        return;
    }
    pthread_mutex_lock(&g_lock);
    uint64_t now = pc_host_ns();
    if (!g_main_wake && deadline_ns > now)
        nap(&g_main_cv, deadline_ns - now);
    g_main_wake = 0;
    pthread_mutex_unlock(&g_lock);
}

void pc_os_wake_main(void)
{
    pthread_mutex_lock(&g_lock);
    g_main_wake = 1;
    pthread_cond_signal(&g_main_cv);
    pthread_mutex_unlock(&g_lock);
}
#endif

void pc_os_dump_threads(void)
{
    static const char *names[] = {[TS_STOPPED] = "stopped", [TS_RUNNABLE] = "runnable", [TS_RUNNING] = "running",
                                  [TS_WAITING] = "waiting", [TS_DEAD] = "dead"};
    pthread_mutex_lock(&g_lock);
    fprintf(stderr, "rush2049pc: N64 threads (baton: %s)\n", g_running ? g_running->name : "none");
    for (int i = 0; i < g_nthreads; i++) {
        PcThread *t = &g_threads[i];
        if (!t->key && !t->boot)
            continue;
        fprintf(stderr, "  %-10s pri %3d %-8s", t->name, (int)t->pri, names[t->state]);
        if (t->wait_q)
            fprintf(stderr, " on queue %p (%s, %d/%d)", t->wait_q->key, t->wait_send ? "send" : "recv",
                    (int)t->wait_q->valid, (int)t->wait_q->count);
        fprintf(stderr, "%s\n", t->event_wait ? " (waiting for events)" : "");
    }
    pthread_mutex_unlock(&g_lock);
}
