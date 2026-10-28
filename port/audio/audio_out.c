/* port/audio/audio_out.c: the output queue behind osAiSetFrequency / osAiSetNextBuffer / osAiGetLength.
 *
 * pc_audio_push takes the game's AI buffers (interleaved stereo s16, native order, at the AI rate), tees them
 * to the dump file, converts them to the device rate and appends them to a one-second ring. Backends drain
 * the ring with pc_audio_pull; the null backend drains it by the clock, so queue lengths behave as with a
 * device. When the ring is full the oldest frames go.
 *
 * The AI (port/ultra/rcp.c) keeps pc_audio_target_frames() queued. With a device (or the tests' manual pulls) the
 * target is two of its pulls, a retrace and a buffer (at least 384 frames), and it grows by a buffer at each
 * underrun, up to 200 ms; audio.latency sets a floor.
 * The pull hook lets the AI ask the game for buffers from the device's own thread, so the main thread's waits
 * (a vsync swap, a long frame) don't starve the device. An underrun plays silence, faded from the last frame
 * over 32 frames, and the sound after it fades in over 32: no repeated or torn buffer, and no click. Silence
 * while the game pushes nothing (before its first buffer, paused, a long load) is counted apart from
 * underruns. */
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
#define TARGET_MIN 384u       /* game frames: two of the game's buffers, the N64's AI depth */
#define TARGET_STEP 192u      /* one buffer more after each underrun */
#define TARGET_MAX_MS 200u
#define FADE 32u              /* frames */
#define PLAYING_S 0.25        /* a push this recent: an empty queue is an underrun, not the game being quiet */
#define REPORT_MAX 10u        /* underruns logged one by one */
#define RING_MAX 32           /* AI buffers tracked (the game has 8) */
#define RING_RECENT 64u       /* a buffer neither rendered nor pushed in this many of those is forgotten */

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
    double last_push;                 /* when the game last pushed (clock_now) */
    int pushed;                       /* the game has pushed since the output opened */
    int dry;                          /* the last pull or drain ran out */
    uint32_t target;                  /* game frames the AI keeps queued (pc_audio_target_frames) */
    uint32_t min_ms;                  /* pc_audio_set_latency */
    int16_t last[2];                  /* the last frame played: an underrun's fade starts there */
    uint32_t fade_in;                 /* frames of fade-in left after an underrun */
    void (*pull_hook)(void);
    uint32_t reported;                /* underruns logged so far */
    int paused;                       /* pc_audio_set_paused: the game is held, its silence isn't an underrun */
    struct {                          /* the game's AI buffers: rendered since they were last pushed? */
        const int16_t *buf;
        int fresh;
        uint32_t op;
    } bufs[RING_MAX];
    uint32_t ops;
    int rendered;                     /* a task has rendered a buffer since the output opened */
    double t0;                        /* when the output opened (clock_now) */
} g = {.mutex = PC_LOCK_INIT, .gain = 65536, .target = TARGET_MIN};

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

static double (*g_clock)(void);   /* pc_audio_set_clock */

static double clock_now(void)
{
    return g_clock ? g_clock() : now_seconds();
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

static uint32_t game_frames(uint64_t device_frames)
{
    return g.dev_rate && g.src_rate ? (uint32_t)(device_frames * g.src_rate / g.dev_rate) : (uint32_t)device_frames;
}

/* The AI's target: from the device's pulls when it has them, at least the latency asked for, never below what it
 * has grown to. */
static void retarget(void)
{
    uint32_t rate = g.src_rate ? g.src_rate : DEFAULT_RATE;
    uint32_t max = rate * TARGET_MAX_MS / 1000 > TARGET_MIN ? rate * TARGET_MAX_MS / 1000 : TARGET_MIN;
    uint32_t t = TARGET_MIN;
    if (g.stats.period_frames)
        t = game_frames(2 * (uint64_t)g.stats.period_frames) + rate / 60 + TARGET_STEP;
    if (rate * g.min_ms / 1000 > t)
        t = rate * g.min_ms / 1000;
    if (t > g.target)
        g.target = t;
    if (g.target > max)
        g.target = max;
    g.stats.target_frames = g.target;
}

/* n device frames found the queue empty. Returns 1 when that starts an underrun: the game was playing. */
static int ran_dry(size_t n, double now)
{
    int start = 0;
    if (g.pushed && !g.paused && now - g.last_push < PLAYING_S) {
        g.stats.underrun_frames += n;
        start = !g.dry;
        g.stats.underruns += (uint64_t)start;
    } else {
        g.stats.idle_frames += n;
    }
    g.dry = 1;
    return start;
}

/* buf was rendered (fresh) or pushed. A push of a buffer known and not rendered since it was last pushed plays
 * the sound of the game's ring's last turn again (the game rendered late: stats.replayed). */
static void note_buffer(const int16_t *buf, int fresh)
{
    int at = 0, found = 0;
    g.ops++;
    for (int i = 0; i < RING_MAX; i++) {
        if (g.bufs[i].buf == buf) {
            at = i;
            found = 1;
            break;
        }
        if (g.bufs[i].op < g.bufs[at].op)
            at = i;
    }
    if (found && !fresh && g.ops - g.bufs[at].op < RING_RECENT && !g.bufs[at].fresh)
        g.stats.replayed++;
    g.bufs[at].buf = buf;
    g.bufs[at].fresh = fresh;
    g.bufs[at].op = g.ops;
}

void pc_audio_set_clock(double (*now)(void))
{
    lock();
    g_clock = now;
    g.clock_last = g.clock_carry = 0;
    g.t0 = clock_now();
    unlock();
}

static void null_drain(void)
{
    if (g.kind != KIND_NULL || !g.dev_rate)
        return;
    double t = clock_now();
    if (g.clock_last > 0) {
        double frames = (t - g.clock_last) * g.dev_rate + g.clock_carry;
        size_t n = (size_t)frames;
        g.clock_carry = frames - (double)n;
        if (n > g.count) {
            ran_dry(n - g.count, t);
            g.rd = g.count = 0;
        } else if (n) {
            g.rd = (g.rd + n) % g.cap;
            g.count -= n;
            g.dry = 0;
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
    g.stats.device_rate = g.dev_rate;
    g.stats.game_rate = src;
    retarget();
}

/* The run's output in one line, for the log. */
static void report(void)
{
    const PcAudioStats *st = &g.stats;
    double rate = g.dev_rate ? g.dev_rate : 1, src = g.src_rate ? g.src_rate : DEFAULT_RATE;
    fprintf(stderr,
            "audio: %s at %u Hz: %.1f s played; %llu underruns (%.0f ms of silence), %llu buffers played again, "
            "%.0f ms idle, %llu frames dropped; pulls of %u frames, queue target %u frames (%.0f ms), at most %.0f ms "
            "queued\n",
            g.name, (unsigned)g.dev_rate, st->pushed_frames / src, (unsigned long long)st->underruns,
            st->underrun_frames * 1000.0 / rate, (unsigned long long)st->replayed, st->idle_frames * 1000.0 / rate,
            (unsigned long long)st->dropped_frames, (unsigned)st->period_frames, (unsigned)st->target_frames,
            st->target_frames * 1000.0 / src, st->max_queued * 1000.0 / rate);
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
    g.pushed = g.dry = 0;
    g.target = TARGET_MIN;
    g.last[0] = g.last[1] = 0;
    g.fade_in = 0;
    g.reported = 0;
    g.t0 = clock_now();
    memset(g.bufs, 0, sizeof g.bufs);
    g.ops = 0;
    g.rendered = 0;
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
    if (g.stats.pushed_frames)
        report();
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
    if (stereo_frames) {
        g.pushed = 1;
        g.last_push = clock_now();
        if (g.rendered)
            note_buffer(samples, 0);
    }
    while (stereo_frames && g.tmp) {
        size_t n = stereo_frames > RS_CHUNK ? RS_CHUNK : stereo_frames;
        size_t made = rs_process(&g.rs, samples, n, g.tmp);
        ring_write(g.tmp, made);
        samples += 2 * n;
        stereo_frames -= n;
    }
    if (g.count > g.stats.max_queued)
        g.stats.max_queued = (uint32_t)g.count;
    unlock();
}

size_t pc_audio_queued_frames(void)
{
    size_t n;
    ensure_init();
    lock();
    null_drain();
    n = game_frames(g.count);
    unlock();
    return n;
}

/* The interpreter wrote one of the game's AI buffers (musyx.c). */
void pc_audio_note_rendered(const int16_t *buf)
{
    lock();
    note_buffer(buf, 1);
    g.rendered = 1;
    unlock();
}

size_t pc_audio_target_frames(void)
{
    size_t t;
    lock();
    t = g.paused ? 0 : g.target;
    unlock();
    return t;
}

/* The main loop holds the game (the pause key, the settings overlay): the AI asks for nothing, and the queue plays
 * out into silence that is not an underrun. */
void pc_audio_set_paused(int paused)
{
    lock();
    g.paused = paused != 0;
    unlock();
}

/* ---------------------------------------------------------------- backends' side */

size_t pc_audio_pull(int16_t *out, size_t frames)
{
    size_t got;
    int32_t gain;
    int log_it = 0;
    uint64_t underruns = 0;
    double at = 0;
    void (*hook)(void);

    lock();
    got = g.ring ? ring_read(out, frames) : 0;
    if (g.kind != KIND_NULL && frames > g.stats.period_frames && frames < g.dev_rate) {   /* a device's pull */
        g.stats.period_frames = (uint32_t)frames;
        retarget();
    }
    if (got && g.fade_in) {   /* after an underrun the sound comes back from silence */
        uint32_t n = got < g.fade_in ? (uint32_t)got : g.fade_in;
        for (uint32_t i = 0; i < n; i++) {
            int32_t k = (int32_t)(FADE - g.fade_in + i + 1);
            out[2 * i] = (int16_t)(out[2 * i] * k / (int32_t)FADE);
            out[2 * i + 1] = (int16_t)(out[2 * i + 1] * k / (int32_t)FADE);
        }
        g.fade_in -= n;
    }
    if (got) {
        g.last[0] = out[2 * got - 2];
        g.last[1] = out[2 * got - 1];
        g.dry = 0;
    }
    if (got < frames) {
        size_t n = frames - got, f = n < FADE ? n : FADE;
        double now = clock_now();
        int16_t *z = out + 2 * got;
        if (ran_dry(n, now)) {
            if (g.kind != KIND_NULL) {   /* the queue was too short for this device and host */
                g.target += TARGET_STEP;
                retarget();
            }
            log_it = g.reported < REPORT_MAX;
            g.reported += (uint32_t)log_it;
            underruns = g.stats.underruns;
            at = now - g.t0;
        }
        /* the last frame fades to silence: a step to zero would click */
        for (size_t i = 0; i < f; i++) {
            int32_t k = (int32_t)(FADE - 1 - i);
            z[2 * i] = (int16_t)(g.last[0] * k / (int32_t)FADE);
            z[2 * i + 1] = (int16_t)(g.last[1] * k / (int32_t)FADE);
        }
        memset(z + 2 * f, 0, (n - f) * 4);
        g.last[0] = g.last[1] = 0;
        g.fade_in = FADE;
    }
    g.stats.pulls++;
    gain = g.gain;
    hook = g.pull_hook;
    unlock();

    if (gain != 65536)   /* full volume leaves the samples as they are */
        for (size_t i = 0; i < 2 * frames; i++)
            out[i] = (int16_t)(out[i] * gain >> 16);
    if (log_it)
        fprintf(stderr, "audio: underrun %llu, %.1f s in: the queue target is now %u frames%s\n",
                (unsigned long long)underruns, at, (unsigned)pc_audio_target_frames(),
                underruns == REPORT_MAX ? " (the exit line counts the rest)" : "");
    if (hook)
        hook();
    return got;
}

void pc_audio_set_pull_hook(void (*hook)(void))
{
    lock();
    g.pull_hook = hook;
    unlock();
}

/* The least the queue keeps, in milliseconds (audio.latency); 0 leaves it to the device's pulls and underruns. */
void pc_audio_set_latency(unsigned ms)
{
    lock();
    g.min_ms = ms > TARGET_MAX_MS ? TARGET_MAX_MS : ms;
    g.target = TARGET_MIN;
    retarget();
    unlock();
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
