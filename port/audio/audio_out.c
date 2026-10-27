/* port/audio/audio_out.c: the output queue behind osAiSetFrequency / osAiSetNextBuffer / osAiGetLength.
 *
 * pc_audio_push takes the game's AI buffers (interleaved stereo s16, native order, at the AI rate), tees them
 * to the dump file, converts them to the device rate and appends them to a one-second ring. Backends drain
 * the ring with pc_audio_pull; the null backend drains it by the clock, so queue lengths behave as with a
 * device. When the ring is full the oldest frames go. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
typedef SRWLOCK PcLock;
#define PC_LOCK_INIT SRWLOCK_INIT
#define lock() AcquireSRWLockExclusive(&g.mutex)
#define unlock() ReleaseSRWLockExclusive(&g.mutex)
#else
#include <pthread.h>
#include <time.h>
typedef pthread_mutex_t PcLock;
#define PC_LOCK_INIT PTHREAD_MUTEX_INITIALIZER
#define lock() pthread_mutex_lock(&g.mutex)
#define unlock() pthread_mutex_unlock(&g.mutex)
#endif

#include "out_backend.h"
#include "out_wav.h"
#include "pc_audio.h"
#include "resample.h"

#define DEFAULT_RATE 22050u   /* what the game asks for (audio_init_sys) */

enum { KIND_NULL, KIND_MANUAL, KIND_DEVICE };

static struct {
    PcLock mutex;
    int inited;
    int kind;
    const PcAudioBackendOps *dev;
    char name[16];
    uint32_t src_rate, dev_rate, forced_rate;
    PcResampler rs;
    int16_t *ring;   /* device-rate frames */
    size_t cap, rd, count;
    int16_t *tmp;
    size_t tmp_cap;
    PcWav dump;
    PcAudioStats stats;
    double clock_last, clock_carry;   /* null backend */
    int32_t gain;                     /* the master volume, Q16 (pc_audio_set_volume) */
} g = {.mutex = PC_LOCK_INIT, .gain = 65536};

static double now_seconds(void)
{
#if defined(_WIN32)
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

/* ---------------------------------------------------------------- with the lock held */

static void ring_reset(uint32_t rate)
{
    size_t cap = rate > 8000 ? rate : 8000;
    if (cap != g.cap) {
        free(g.ring);
        g.ring = malloc(cap * 4);
        g.cap = g.ring ? cap : 0;
    }
    g.rd = g.count = 0;
}

static void ring_write(const int16_t *s, size_t n)
{
    if (!g.cap)
        return;
    if (n > g.cap) {   /* keep the newest */
        g.stats.dropped_frames += n - g.cap;
        s += 2 * (n - g.cap);
        n = g.cap;
    }
    if (g.count + n > g.cap) {
        size_t drop = g.count + n - g.cap;
        g.rd = (g.rd + drop) % g.cap;
        g.count -= drop;
        g.stats.dropped_frames += drop;
    }
    size_t wr = (g.rd + g.count) % g.cap;
    size_t first = g.cap - wr < n ? g.cap - wr : n;
    memcpy(g.ring + 2 * wr, s, first * 4);
    memcpy(g.ring, s + 2 * first, (n - first) * 4);
    g.count += n;
}

static size_t ring_read(int16_t *out, size_t n)
{
    size_t got = n < g.count ? n : g.count;
    size_t first = g.cap - g.rd < got ? g.cap - g.rd : got;
    if (got) {
        memcpy(out, g.ring + 2 * g.rd, first * 4);
        memcpy(out + 2 * first, g.ring, (got - first) * 4);
        g.rd = (g.rd + got) % g.cap;
        g.count -= got;
    }
    return got;
}

static double (*g_clock)(void);   /* pc_audio_set_clock */

void pc_audio_set_clock(double (*now)(void))
{
    lock();
    g_clock = now;
    g.clock_last = g.clock_carry = 0;
    unlock();
}

static void null_drain(void)
{
    if (g.kind != KIND_NULL || !g.dev_rate)
        return;
    double t = g_clock ? g_clock() : now_seconds();
    if (g.clock_last > 0) {
        double frames = (t - g.clock_last) * g.dev_rate + g.clock_carry;
        size_t n = (size_t)frames;
        g.clock_carry = frames - (double)n;
        if (n >= g.count) {
            g.stats.underrun_frames += n - g.count;
            g.rd = g.count = 0;
        } else {
            g.rd = (g.rd + n) % g.cap;
            g.count -= n;
        }
    }
    g.clock_last = t;
}

/* The game's rate is known (or assumed): settle the device rate, the converter and the ring. */
static void configure(uint32_t src)
{
    g.src_rate = src;
    if (g.kind != KIND_DEVICE)
        g.dev_rate = g.forced_rate ? g.forced_rate : src;
    rs_init(&g.rs, g.src_rate, g.dev_rate);
    size_t need = rs_max_out(&g.rs, RS_CHUNK);
    if (need > g.tmp_cap) {
        free(g.tmp);
        g.tmp = malloc(need * 4);
        g.tmp_cap = g.tmp ? need : 0;
    }
    if (g.cap < g.dev_rate || !g.ring)
        ring_reset(g.dev_rate);
    pc_wav_set_rate(&g.dump, src);
}

/* ---------------------------------------------------------------- setup */

static const PcAudioBackendOps *device_named(const char *name)
{
#ifdef PC_AUDIO_HAVE_SDL2
    if (!strcmp(name, "sdl"))
        return &pc_audio_backend_sdl;
#endif
#ifdef PC_AUDIO_HAVE_ALSA
    if (!strcmp(name, "alsa"))
        return &pc_audio_backend_alsa;
#endif
    (void)name;
    return NULL;
}

int pc_audio_init(const char *backend, const char *dump_wav_path)
{
    const char *want = backend && *backend ? backend : "auto";
    const char *env_rate = getenv("RUSH2049_AUDIO_RATE");
    int rc = 0;
    uint32_t keep_rate = g.src_rate;

    if (g.inited)
        pc_audio_shutdown();

    lock();
    g.inited = 1;
    g.src_rate = keep_rate;
    g.kind = KIND_NULL;
    g.dev = NULL;
    g.forced_rate = env_rate ? (uint32_t)strtoul(env_rate, NULL, 10) : 0;
    memset(&g.stats, 0, sizeof g.stats);
    g.clock_last = g.clock_carry = 0;
    if (dump_wav_path && *dump_wav_path) {
        if (pc_wav_open(&g.dump, dump_wav_path, g.src_rate ? g.src_rate : DEFAULT_RATE) != 0) {
            fprintf(stderr, "audio: can't write %s\n", dump_wav_path);
            rc = -1;
        }
    } else if (!strcmp(want, "wav")) {
        fprintf(stderr, "audio: the wav backend needs a dump path (--dump-audio)\n");
        rc = -1;
    }
    unlock();

    /* A device opens at the game's rate when it is known, else at the rate the game asks for. */
    uint32_t want_rate = g.forced_rate ? g.forced_rate : g.src_rate ? g.src_rate : DEFAULT_RATE;
    const char *order[3] = {NULL, NULL, NULL};
    int is_device = !strcmp(want, "sdl") || !strcmp(want, "alsa");
    if (!strcmp(want, "auto")) {
        order[0] = "sdl";
        order[1] = "alsa";
    } else if (is_device) {
        order[0] = want;
    } else if (strcmp(want, "null") && strcmp(want, "wav") && strcmp(want, "manual")) {
        fprintf(stderr, "audio: unknown backend \"%s\", using null\n", want);
        rc = -1;
    }
    for (int i = 0; i < 3 && order[i]; i++) {
        const PcAudioBackendOps *ops = device_named(order[i]);
        uint32_t got = 0;
        if (ops && ops->open(want_rate, &got) == 0 && got) {
            lock();
            g.kind = KIND_DEVICE;
            g.dev = ops;
            g.dev_rate = got;
            unlock();
            break;
        }
        if (is_device) {
            fprintf(stderr, "audio: backend \"%s\" %s, using null\n", want, ops ? "didn't open" : "isn't built");
            rc = -1;
        }
    }

    lock();
    if (g.kind != KIND_DEVICE && !strcmp(want, "manual"))
        g.kind = KIND_MANUAL;
    snprintf(g.name, sizeof g.name, "%s",
             g.kind == KIND_DEVICE ? g.dev->name : g.kind == KIND_MANUAL ? "manual" : g.dump.f ? "wav" : "null");
    configure(g.src_rate ? g.src_rate : DEFAULT_RATE);
    fprintf(stderr, "audio: output %s at %u Hz%s%s\n", g.name, (unsigned)g.dev_rate,
            g.dump.f ? ", dumping to " : "", g.dump.f ? dump_wav_path : "");
    unlock();
    return rc;
}

static void ensure_init(void)
{
    if (!g.inited)
        pc_audio_init(getenv("RUSH2049_AUDIO"), getenv("RUSH2049_DUMP_AUDIO"));
}

void pc_audio_shutdown(void)
{
    const PcAudioBackendOps *dev;
    if (!g.inited)
        return;
    lock();
    dev = g.kind == KIND_DEVICE ? g.dev : NULL;
    g.kind = KIND_NULL;
    g.dev = NULL;
    unlock();
    if (dev)
        dev->close();
    lock();
    pc_wav_close(&g.dump);
    free(g.ring);
    free(g.tmp);
    g.ring = g.tmp = NULL;
    g.cap = g.tmp_cap = g.rd = g.count = 0;
    g.src_rate = g.dev_rate = 0;
    g.inited = 0;
    unlock();
}

/* ---------------------------------------------------------------- the game's side */

void pc_audio_set_frequency(uint32_t hz)
{
    ensure_init();
    if (!hz)
        return;
    lock();
    if (hz != g.src_rate) {
        configure(hz);
        if (g.kind != KIND_DEVICE)
            ring_reset(g.dev_rate);   /* queued frames were at the old rate */
    }
    unlock();
}

void pc_audio_push(const int16_t *samples, size_t stereo_frames)
{
    ensure_init();
    lock();
    if (!g.src_rate)
        configure(DEFAULT_RATE);
    null_drain();
    pc_wav_write(&g.dump, samples, stereo_frames);
    g.stats.pushed_frames += stereo_frames;
    while (stereo_frames && g.tmp) {
        size_t n = stereo_frames > RS_CHUNK ? RS_CHUNK : stereo_frames;
        size_t made = rs_process(&g.rs, samples, n, g.tmp);
        ring_write(g.tmp, made);
        samples += 2 * n;
        stereo_frames -= n;
    }
    unlock();
}

size_t pc_audio_queued_frames(void)
{
    size_t n;
    ensure_init();
    lock();
    null_drain();
    n = g.dev_rate && g.src_rate ? (size_t)((uint64_t)g.count * g.src_rate / g.dev_rate) : g.count;
    unlock();
    return n;
}

/* ---------------------------------------------------------------- backends' side */

size_t pc_audio_pull(int16_t *out, size_t frames)
{
    size_t got;
    int32_t gain;
    lock();
    got = g.ring ? ring_read(out, frames) : 0;
    if (got < frames)
        g.stats.underrun_frames += frames - got;
    gain = g.gain;
    unlock();
    if (gain != 65536)   /* full volume leaves the samples as they are */
        for (size_t i = 0; i < 2 * got; i++)
            out[i] = (int16_t)(out[i] * gain >> 16);
    memset(out + 2 * got, 0, (frames - got) * 4);
    return got;
}

/* The volume is a slider: its square is the gain, so 50 is about -12 dB and 10 about -40 dB. It applies where the
 * backends pull, so the dump file (--dump-audio) keeps what the game played. */
void pc_audio_set_volume(double volume)
{
    double v = volume < 0 ? 0 : volume > 1 ? 1 : volume;
    lock();
    g.gain = (int32_t)(v * v * 65536 + 0.5);
    unlock();
}

uint32_t pc_audio_device_rate(void)
{
    uint32_t r;
    lock();
    r = g.dev_rate;
    unlock();
    return r;
}

const char *pc_audio_backend_name(void)
{
    return g.inited ? g.name : "none";
}

void pc_audio_get_stats(PcAudioStats *stats)
{
    lock();
    *stats = g.stats;
    unlock();
}
