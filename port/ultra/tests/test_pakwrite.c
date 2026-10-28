/* port/ultra/tests/test_pakwrite.c: the paks' writer thread (port/ultra/pakwrite.c, docs/port/ULTRA.md "Controller
 * pak") with a fake write that can be slow, held, or fail: a submit returns at once while a write takes 200 ms; a
 * newer image replaces a queued one; the files are written in the order they changed; never two writes at once; a
 * failed write is kept until pc_pakw_retry, and dropped for a newer image; the wait at the end; the fault path; and
 * the real atomic write into a folder (argv[1]). Exit status: the number of checks that failed. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "ultra.h"

static int g_failed;
#define CHECK(cond, what)                                                         \
    do {                                                                          \
        if (cond)                                                                 \
            printf("ok    %s\n", what);                                           \
        else                                                                      \
            printf("FAIL  %s (%s:%d)\n", what, __FILE__, __LINE__), g_failed++;  \
    } while (0)

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void nap_ms(int ms)
{
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

/* ---------------------------------------------------------------- the fake write */

static pthread_mutex_t f_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t f_cv = PTHREAD_COND_INITIALIZER;
static int f_delay_ms;        /* each write takes this long */
static int f_hold;            /* writes wait while set */
static int f_entered;         /* writes that have started */
static char f_fail[64];       /* writes of this path fail (EACCES) */
static int f_active, f_max_active;
static struct {
    char path[64];
    int version;              /* the image's first byte */
} f_log[64];
static int f_n;

static int fake_write(const char *path, const void *data, size_t size)
{
    pthread_mutex_lock(&f_lock);
    f_entered++;
    if (++f_active > f_max_active)
        f_max_active = f_active;
    pthread_cond_broadcast(&f_cv);
    while (f_hold)
        pthread_cond_wait(&f_cv, &f_lock);
    int delay = f_delay_ms, fail = !strcmp(path, f_fail);
    pthread_mutex_unlock(&f_lock);
    if (delay)
        nap_ms(delay);
    pthread_mutex_lock(&f_lock);
    if (f_n < 64) {
        snprintf(f_log[f_n].path, sizeof f_log[f_n].path, "%s", path);
        f_log[f_n].version = size ? ((const unsigned char *)data)[0] : -1;
        f_n++;
    }
    f_active--;
    pthread_mutex_unlock(&f_lock);
    if (fail) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

static void reset(void)
{
    pthread_mutex_lock(&f_lock);
    f_delay_ms = 0, f_hold = 0, f_entered = 0, f_fail[0] = 0, f_n = 0, f_max_active = 0;
    pthread_cond_broadcast(&f_cv);
    pthread_mutex_unlock(&f_lock);
}

static void hold(int on)
{
    pthread_mutex_lock(&f_lock);
    f_hold = on;
    pthread_cond_broadcast(&f_cv);
    pthread_mutex_unlock(&f_lock);
}

/* until n writes have started (the writer is inside the fake write), at most 2 s */
static int entered(int n)
{
    uint64_t end = now_ns() + 2000000000u;
    pthread_mutex_lock(&f_lock);
    while (f_entered < n && now_ns() < end) {
        pthread_mutex_unlock(&f_lock);
        nap_ms(1);
        pthread_mutex_lock(&f_lock);
    }
    int ok = f_entered >= n;
    pthread_mutex_unlock(&f_lock);
    return ok;
}

static unsigned char g_img[PC_PAK_SIZE];

static int submit(const char *path, int version)
{
    g_img[0] = (unsigned char)version;
    return pc_pakw_submit(path, "the test's pak", g_img, sizeof g_img);
}

/* the fake write's log as "path:version path:version ..." */
static const char *written(void)
{
    static char out[1024];
    size_t n = 0;
    out[0] = 0;
    pthread_mutex_lock(&f_lock);
    for (int i = 0; i < f_n && n < sizeof out; i++)
        n += (size_t)snprintf(out + n, sizeof out - n, "%s%s:%d", i ? " " : "", f_log[i].path, f_log[i].version);
    pthread_mutex_unlock(&f_lock);
    return out;
}

/* Each file's writes in the log: its versions only ever newer, the last one last_version. */
static int newer_only(const char *path, int last_version)
{
    int last = -1, ok = 1;
    pthread_mutex_lock(&f_lock);
    for (int i = 0; i < f_n; i++)
        if (!strcmp(f_log[i].path, path)) {
            ok &= f_log[i].version > last;
            last = f_log[i].version;
        }
    pthread_mutex_unlock(&f_lock);
    return ok && last == last_version;
}

/* ---------------------------------------------------------------- the checks */

static void not_blocked(void)
{
    reset();
    f_delay_ms = 200;
    uint64_t t0 = now_ns();
    int r = submit("a", 1);
    uint64_t took = now_ns() - t0;
    CHECK(r == 0 && took < 20000000u, "a submit returns at once while the write takes 200 ms");
    CHECK(entered(1), "the writer has started the write");
    uint64_t worst = 0;
    for (int i = 2; i < 40; i++) {   /* the main loop's flushes meanwhile, a frame apart */
        t0 = now_ns();
        submit(i & 1 ? "b" : "a", i);
        if (now_ns() - t0 > worst)
            worst = now_ns() - t0;
        nap_ms(2);
    }
    char what[200];
    snprintf(what, sizeof what, "38 more submits while it writes, none held (the longest %.2f ms)", worst / 1e6);
    CHECK(worst < 20000000u, what);
    t0 = now_ns();
    int left = pc_pakw_wait(0, 5000000000u);
    CHECK(left == 0, "the wait at the end: everything written");
    CHECK(now_ns() - t0 >= 150000000u, "the wait waited for the slow write");
    /* a's first write, then each file's newer images only, the latest last (a single later write each, unless the
     * submits outlasted the 200 ms write) */
    snprintf(what, sizeof what, "each file's images only newer, ending with its latest (%s)", written());
    CHECK(newer_only("a", 38) && newer_only("b", 39) && f_n < 10, what);
    CHECK(f_max_active == 1, "never two writes at once");
}

static void supersede_and_order(void)
{
    PcPakWriteStats st0;
    pc_pakw_stats(&st0);
    reset();
    hold(1);
    submit("A", 1);
    CHECK(entered(1), "A's first write started (held)");
    submit("B", 1);
    submit("C", 1);
    submit("A", 2);
    submit("B", 2);   /* replaces B 1, in B's place */
    submit("A", 3);   /* replaces A 2, behind C */
    PcPakWriteStats st;
    pc_pakw_stats(&st);
    char what[300];
    hold(0);
    int left = pc_pakw_wait(0, 5000000000u);
    snprintf(what, sizeof what, "order kept, newer images in their place: %s", written());
    CHECK(left == 0 && !strcmp(written(), "A:1 B:2 C:1 A:3"), what);
    CHECK(f_max_active == 1, "one write at a time");
    CHECK(st.superseded - st0.superseded == 2, "two images replaced before they were written");
}

static void failure_and_retry(void)
{
    char what[300];
    reset();
    snprintf(f_fail, sizeof f_fail, "F");
    submit("F", 1);
    int left = pc_pakw_wait(0, 5000000000u);
    CHECK(left == 1 && !strcmp(written(), "F:1"), "a failed write: the file not written, its image kept");
    char names[200];
    CHECK(pc_pakw_unwritten(names, sizeof names) == 1 && strstr(names, "(F)"), "named as not written");
    nap_ms(100);
    CHECK(!strcmp(written(), "F:1"), "not tried again on its own");
    pc_pakw_retry();
    left = pc_pakw_wait(0, 5000000000u);
    CHECK(left == 1 && !strcmp(written(), "F:1 F:1"), "pc_pakw_retry: tried again (still failing)");
    pthread_mutex_lock(&f_lock);
    f_fail[0] = 0;
    pthread_mutex_unlock(&f_lock);
    left = pc_pakw_wait(1, 5000000000u);
    snprintf(what, sizeof what, "the wait with retry: written once the write works (%s)", written());
    CHECK(left == 0 && !strcmp(written(), "F:1 F:1 F:1"), what);

    /* a failure while a newer image waits: the newer one is written, the failed one dropped */
    reset();
    snprintf(f_fail, sizeof f_fail, "G");
    hold(1);
    submit("G", 1);
    CHECK(entered(1), "G's write started (held, to fail)");
    submit("G", 2);
    hold(0);
    CHECK(entered(2), "G 2 written next");
    pthread_mutex_lock(&f_lock);
    f_fail[0] = 0;   /* G 2's write may have started already (failing too): then the retry below writes it */
    pthread_mutex_unlock(&f_lock);
    left = pc_pakw_wait(1, 5000000000u);
    snprintf(what, sizeof what, "a failed image never comes back after a newer one (%s)", written());
    const char *w = written();
    CHECK(left == 0 && (!strcmp(w, "G:1 G:2") || !strcmp(w, "G:1 G:2 G:2")), what);
}

static void wait_at_end(void)
{
    reset();
    f_delay_ms = 100;
    submit("x", 1);
    submit("y", 1);
    submit("z", 1);
    int left = pc_pakw_wait(0, 50000000u);
    CHECK(left > 0, "a wait that times out says what is left");
    uint64_t t0 = now_ns();
    left = pc_pakw_wait(1, 5000000000u);
    CHECK(left == 0 && !strcmp(written(), "x:1 y:1 z:1"), "the wait at the end: every file written, in order");
    CHECK(now_ns() - t0 >= 150000000u, "it waited for the slow writes");
}

static void fault_path(void)
{
    reset();
    f_delay_ms = 50;
    g_img[0] = 7;
    CHECK(pc_pakw_fault_submit("f", "the test's pak", g_img, sizeof g_img) == 0, "a submit after a fault");
    CHECK(pc_pakw_fault_wait(2000000000u) == 0 && !strcmp(written(), "f:7"),
          "the wait after a fault, without the lock: written");
    CHECK(!pc_pakw_is_writer(), "the test's thread isn't the writer");
}

static void real_write(const char *dir)
{
    char path[512];
    unsigned char back[PC_PAK_SIZE];
    pc_pakw_set_write(NULL);
    mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/pak writer.mpk", dir);
    remove(path);
    pc_pak_format(g_img);
    CHECK(pc_pakw_submit(path, "the test's pak", g_img, sizeof g_img) == 0 && pc_pakw_wait(0, 5000000000u) == 0,
          "pc_file_write_atomic on the writer's thread");
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(back, 1, sizeof back, f) : 0;
    if (f)
        fclose(f);
    CHECK(n == sizeof back && !memcmp(back, g_img, sizeof back) &&
              pc_pak_image_check(back, n, NULL, 0) == PC_PAK_IMAGE_OK,
          "the file is the image, a good pak");
    remove(path);
    pc_pakw_set_write(fake_write);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    pc_pakw_set_write(fake_write);
    not_blocked();
    supersede_and_order();
    failure_and_retry();
    wait_at_end();
    fault_path();
    real_write(argc > 1 ? argv[1] : "test-scratch-pakwrite");
    printf("%s: %d failed\n", argv[0], g_failed);
    return g_failed;
}
