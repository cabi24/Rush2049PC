/* port/ultra/pakwrite.c: the controller paks' files written on a thread of their own (docs/port/ULTRA.md "Controller
 * pak", RELEASE.md "Saves").
 *
 * pc_file_write_atomic (pakfile.c) flushes the file and its folder to the disk: under 20 ms on a fast disk, much more
 * on a laptop's hard disk, a USB stick, a network folder, or while an antivirus scans the file. Done on the main loop
 * it held the retraces and the audio's AI events (a stutter, the sound dropping), so si.c hands each changed image
 * here instead, and this thread writes it:
 *   - pc_pakw_submit copies the image and returns at once. A newer image of a file replaces one of the same file not
 *     yet written, which keeps its place: the files are written in the order they changed, each with its latest
 *     image, and never an older image after a newer one;
 *   - one thread writes, one file at a time, so a file never has two writes at once;
 *   - a write that fails keeps its image (unless a newer one came meanwhile), said once, until pc_pakw_retry, which
 *     si.c calls at each flush (once a second): tried again then, as before;
 *   - pc_pakw_wait waits until nothing is queued or being written (each failed file tried once more first): at the
 *     end of a run, and after each flush with --fast (si.c).
 * The thread never takes si.c's lock; si.c submits under it (the order: si.c's lock, then this one). Pure: no game
 * code, so the tests link it (port/ultra/tests/test_pakwrite.c). Without a thread (pthread_create failed) a submit
 * writes at once, in the caller. RUSH_SIM_PAK_WRITE_MS holds each write that long first (a slow disk, for the tests:
 * tools/port/pakwrite_check.py); RUSH_SIM_PAK_WRITE_SYNC=1 writes in the caller, on the main loop, as before. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ultra.h"

#define SLOTS 8   /* files: four paks, and room for a pak whose file changed while its old one is written */

enum { CLEAN, QUEUED, FAILED };

typedef struct {
    char *path;              /* NULL: free */
    char label[48];          /* "controller 1's pak", for the messages */
    uint8_t *data, *writing; /* the image to write; the one being written */
    size_t size, cap, wsize, wcap;
    int state;               /* CLEAN, QUEUED (in g_queue), FAILED (its image kept, until pc_pakw_retry) */
    int busy;                /* being written (the image is in writing) */
    int failing;             /* its last write failed: said once, until one succeeds */
} Slot;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_work, g_idle;
static Slot g_slot[SLOTS];
static int g_queue[SLOTS], g_queued;   /* the queued slots, oldest first */
static int g_outstanding;              /* queued + busy (__atomic: read without the lock after a fault) */
static int g_started, g_threadless;
static pthread_t g_thread;
static PcPakWriteFn g_write = pc_file_write_atomic;
static uint64_t g_delay_ns;
static int g_sim_sync;
static PcPakWriteStats g_stats;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void nap_ns(uint64_t ns)
{
    struct timespec ts = {(time_t)(ns / 1000000000u), (long)(ns % 1000000000u)};
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
}

static void count_outstanding(void)
{
    int n = g_queued;
    for (int i = 0; i < SLOTS; i++)
        n += g_slot[i].busy;
    __atomic_store_n(&g_outstanding, n, __ATOMIC_RELEASE);
}

static void enqueue(int i)
{
    g_slot[i].state = QUEUED;
    g_queue[g_queued++] = i;
}

/* A message decided under the lock, printed after it: the log's pipe may be full, and the main loop must not wait
 * for it in pc_pakw_submit. */
typedef struct {
    char text[640];
} Message;

static void say(const Message *m)
{
    if (m->text[0])
        fprintf(stderr, "rush2049pc: [pak writer] %s\n", m->text);
}

/* The oldest queued file written (lock held; released during the write). */
static void write_one(Message *m)
{
    int i = g_queue[0];
    Slot *s = &g_slot[i];
    memmove(g_queue, g_queue + 1, (size_t)--g_queued * sizeof *g_queue);
    uint8_t *t = s->writing;
    size_t tcap = s->wcap;
    s->writing = s->data, s->wcap = s->cap, s->wsize = s->size;
    s->data = t, s->cap = tcap, s->size = 0;
    s->state = CLEAN;
    s->busy = 1;
    count_outstanding();
    const char *path = s->path;   /* a busy slot keeps its path */
    uint64_t delay = g_delay_ns;
    pthread_mutex_unlock(&g_lock);

    uint64_t t0 = now_ns();
    if (delay)
        nap_ns(delay);
    int ok = g_write(path, s->writing, s->wsize) == 0, err = errno;
    uint64_t took = now_ns() - t0;

    pthread_mutex_lock(&g_lock);
    s->busy = 0;
    m->text[0] = 0;
    if (ok) {
        g_stats.writes++;
        if (s->failing)
            snprintf(m->text, sizeof m->text, "%s: %s written", s->label, path);
        s->failing = 0;
    } else {
        g_stats.failures++;
        if (!s->failing)
            snprintf(m->text, sizeof m->text, "%s: cannot write %s: %s (the saves are kept in memory and tried "
                     "again)", s->label, path, strerror(err));
        s->failing = 1;
        if (s->state == CLEAN) {   /* no newer image came meanwhile: this one is kept */
            t = s->data, tcap = s->cap;
            s->data = s->writing, s->cap = s->wcap, s->size = s->wsize;
            s->writing = t, s->wcap = tcap;
            s->state = FAILED;
        }
    }
    if (took > g_stats.longest_ns)
        g_stats.longest_ns = took;
    count_outstanding();
    pthread_cond_broadcast(&g_idle);
}

static void *writer(void *arg)
{
    Message m;
    (void)arg;
    pthread_mutex_lock(&g_lock);
    for (;;) {
        while (!g_queued)
            pthread_cond_wait(&g_work, &g_lock);
        write_one(&m);
        pthread_mutex_unlock(&g_lock);
        say(&m);
        pthread_mutex_lock(&g_lock);
    }
    return NULL;
}

static void init_cv(pthread_cond_t *cv)
{
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(cv, &a);
    pthread_condattr_destroy(&a);
}

/* The thread, at the first submit (lock held). */
static void start(void)
{
    if (g_started || g_threadless)
        return;
    const char *d = getenv("RUSH_SIM_PAK_WRITE_MS"), *s = getenv("RUSH_SIM_PAK_WRITE_SYNC");
    if (d && atof(d) > 0)
        g_delay_ns = (uint64_t)(atof(d) * 1e6);
    g_sim_sync = s && !strcmp(s, "1");
    init_cv(&g_work);
    init_cv(&g_idle);
    if (g_sim_sync || pthread_create(&g_thread, NULL, writer, NULL) != 0) {
        if (!g_sim_sync)
            fprintf(stderr, "rush2049pc: [pak writer] no thread: the paks are written on the main loop\n");
        g_threadless = 1;
        return;
    }
    pthread_detach(g_thread);
    __atomic_store_n(&g_started, 1, __ATOMIC_RELEASE);
}

/* The slot of path (lock held): its own, a free one, or a clean one taken over; -1 when every one is in use. */
static int slot_of(const char *path)
{
    int free_i = -1, clean_i = -1;
    for (int i = 0; i < SLOTS; i++) {
        Slot *s = &g_slot[i];
        if (s->path && !strcmp(s->path, path))
            return i;
        if (!s->path && free_i < 0)
            free_i = i;
        if (s->path && s->state == CLEAN && !s->busy && clean_i < 0)
            clean_i = i;
    }
    int i = free_i >= 0 ? free_i : clean_i;
    if (i < 0)
        return -1;
    char *copy = strdup(path);
    if (!copy)
        return -1;
    free(g_slot[i].path);
    g_slot[i].path = copy;
    g_slot[i].failing = 0;
    return i;
}

/* lock held */
static int put(const char *path, const char *label, const void *data, size_t size)
{
    start();
    int i = slot_of(path);
    if (i < 0)
        return -1;
    Slot *s = &g_slot[i];
    if (s->cap < size) {
        uint8_t *b = realloc(s->data, size);
        if (!b)
            return -1;
        s->data = b, s->cap = size;
    }
    memcpy(s->data, data, size);
    s->size = size;
    snprintf(s->label, sizeof s->label, "%s", label ? label : "the pak");
    g_stats.submits++;
    if (s->state == QUEUED)
        g_stats.superseded++;   /* the newer image takes the older one's place in the line */
    else
        enqueue(i);
    count_outstanding();
    if (!g_threadless)
        pthread_cond_signal(&g_work);
    return 0;
}

/* Without the thread: the queue written here, in the caller (lock held, released for each write and message). */
static void write_here(void)
{
    Message m;
    while (g_threadless && g_queued) {
        write_one(&m);
        pthread_mutex_unlock(&g_lock);
        say(&m);
        pthread_mutex_lock(&g_lock);
    }
}

int pc_pakw_submit(const char *path, const char *label, const void *data, size_t size)
{
    if (!path || !data)
        return -1;
    pthread_mutex_lock(&g_lock);
    int r = put(path, label, data, size);
    write_here();
    pthread_mutex_unlock(&g_lock);
    return r;
}

/* lock held: each failed file queued again, in slot order */
static void requeue_failed(void)
{
    for (int i = 0; i < SLOTS; i++)
        if (g_slot[i].state == FAILED)
            enqueue(i);
    count_outstanding();
}

static int not_written(void)
{
    int n = 0;
    for (int i = 0; i < SLOTS; i++)
        n += g_slot[i].state != CLEAN || g_slot[i].busy;
    return n;
}

void pc_pakw_retry(void)
{
    pthread_mutex_lock(&g_lock);
    requeue_failed();
    if (g_started && g_queued)
        pthread_cond_signal(&g_work);
    write_here();
    pthread_mutex_unlock(&g_lock);
}

int pc_pakw_wait(int retry, uint64_t timeout_ns)
{
    if (retry)
        pc_pakw_retry();
    pthread_mutex_lock(&g_lock);
    uint64_t end = now_ns() + timeout_ns;
    while (g_started) {
        int busy = 0;
        for (int i = 0; i < SLOTS; i++)
            busy |= g_slot[i].busy;
        if (!g_queued && !busy)
            break;
        uint64_t now = now_ns();
        if (now >= end)
            break;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t t = (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec + (end - now);
        ts.tv_sec = (time_t)(t / 1000000000u), ts.tv_nsec = (long)(t % 1000000000u);
        pthread_cond_timedwait(&g_idle, &g_lock, &ts);
    }
    int n = not_written();
    pthread_mutex_unlock(&g_lock);
    return n;
}

int pc_pakw_unwritten(char *what, size_t size)
{
    size_t len = 0;
    if (what && size)
        what[0] = 0;
    pthread_mutex_lock(&g_lock);
    int n = 0;
    for (int i = 0; i < SLOTS; i++) {
        const Slot *s = &g_slot[i];
        if (s->state == CLEAN && !s->busy)
            continue;
        n++;
        if (what && len < size)
            len += (size_t)snprintf(what + len, size - len, "%s%s (%s)", n > 1 ? ", " : "", s->label, s->path);
    }
    pthread_mutex_unlock(&g_lock);
    return n;
}

void pc_pakw_stats(PcPakWriteStats *out)
{
    pthread_mutex_lock(&g_lock);
    *out = g_stats;
    pthread_mutex_unlock(&g_lock);
}

void pc_pakw_set_write(PcPakWriteFn fn)
{
    pthread_mutex_lock(&g_lock);
    g_write = fn ? fn : pc_file_write_atomic;
    pthread_mutex_unlock(&g_lock);
}

int pc_pakw_is_writer(void)
{
    return __atomic_load_n(&g_started, __ATOMIC_ACQUIRE) && pthread_equal(pthread_self(), g_thread);
}

/* After a fault: no blocking on a lock the faulted thread may hold. The lock is tried for up to 50 ms (the main
 * loop or the writer hold it only for a copy); then the queue is left to the thread, polled without the lock. */
int pc_pakw_fault_submit(const char *path, const char *label, const void *data, size_t size)
{
    for (int i = 0; i < 50; i++) {
        if (pthread_mutex_trylock(&g_lock) == 0) {
            int r = put(path, label, data, size);
            write_here();
            pthread_mutex_unlock(&g_lock);
            return r;
        }
        nap_ns(1000000);
    }
    return -1;
}

int pc_pakw_fault_wait(uint64_t timeout_ns)
{
    if (pc_pakw_is_writer())
        return __atomic_load_n(&g_outstanding, __ATOMIC_ACQUIRE);   /* the writer faulted: nothing more is written */
    uint64_t end = now_ns() + timeout_ns;
    while (__atomic_load_n(&g_outstanding, __ATOMIC_ACQUIRE) && now_ns() < end)
        nap_ns(1000000);
    return __atomic_load_n(&g_outstanding, __ATOMIC_ACQUIRE);
}
