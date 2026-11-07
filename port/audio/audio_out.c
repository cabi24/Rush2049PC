/* port/audio/audio_out.c: the output queue behind osAiSetFrequency / osAiSetNextBuffer / osAiGetLength.
 *
 * pc_audio_push takes the game's AI buffers (interleaved stereo s16, native order, at the AI rate), converts them
 * to the device rate and appends them to a one-second ring; the dump file gets them as they enter the converter.
 * Backends drain the ring with pc_audio_pull; the null backend drains it by the clock, so queue lengths behave as
 * with a device. When the ring is full the oldest frames go.
 *
 * The AI (port/ultra/rcp.c) keeps pc_audio_target_frames() queued. With a device (or the tests' manual pulls) the
 * target is two of its pulls, a retrace and a buffer (at least 384 frames), and it grows by a buffer at each
 * underrun, up to 200 ms; audio.latency sets a floor.
 * The pull hook lets the AI ask the game for buffers from the device's own thread, so the main thread's waits
 * (a vsync swap, a long frame) don't starve the device. An underrun plays silence, faded from the last frame
 * over 32 frames, and the sound after it fades in over 32: no repeated or torn buffer, and no click. Silence
 * while the game pushes nothing (before its first buffer, paused, a long load) is counted apart from
 * underruns.
 *
 * Buffers pushed before they were rendered. The game renders its ring of AI buffers from its retrace callback, a
 * retrace after it sets them up and a few at a time (docs/port/AUDIO.md), and the N64's AI reads a buffer
 * only as it plays it. The PC's AI pushes a buffer as the device's queue needs it, which can be before its task has
 * run: copied then, it would play the ring's last turn again. So a push whose render hasn't come waits in the queue
 * (held) until it does, or until the device needs it. Then it plays the buffer's previous render if that never
 * played and comes after what last did, else silence (faded, as an underrun): nothing plays twice, early or out of
 * order. Renders are matched to pushes exactly: the game pushes its ring in order and renders it in order, so each
 * render is for the push after the previous render's, unless the renderer started over (the order jumps, or a whole
 * ring turn passed without a render), where it renders for the ring's next turn (pc_audio_note_rendered). */
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

#include "musyx_abi.h"
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
#define LEVEL_AFTER_S 1.0     /* the queue's level is measured from this long after the first push */
#define REPORT_MAX 10u        /* underruns logged one by one */
#define RING_MAX 32           /* AI buffers tracked (the game has 8) */
#define HOLD_MAX 32           /* pushes held at once */
#define HOLD_FRAMES 1024u     /* a push this long or shorter can be held (the game's are 192 frames) */

enum { KIND_NULL, KIND_MANUAL, KIND_DEVICE };
enum { H_WAITING, H_READY, H_SILENT };

typedef struct {
    const int16_t *buf;
    uint64_t pushed;     /* the index of its last push (0: never pushed) */
    uint64_t render;     /* the push its unplayed render is for (0: none) */
    int fresh;           /* rendered since its last push (before renders are matched) */
    uint64_t op;         /* last use, to forget the least recent */
} Buf;

typedef struct {
    const int16_t *buf;
    uint64_t index;      /* the push's index */
    uint64_t sound;      /* H_READY: the push whose render data holds */
    uint32_t frames;
    int state;           /* H_WAITING for its render, H_READY (data), H_SILENT */
    int16_t data[2 * HOLD_FRAMES];
} Held;

static struct {
    PcLock mutex;
    int inited;
    int closed;          /* pc_audio_shutdown ran: the game's threads still push, and mustn't open it again */
    int kind;
    const PcAudioBackendOps *dev;
    char name[16];
    PcAudioDevice device;             /* what a device backend opened as */
    uint32_t src_rate, dev_rate, forced_rate;
    PcResampler rs;
    int32_t *ring;   /* device-rate frames, past full scale where the converter overshoots */
    size_t cap, rd, count;
    int32_t *tmp;
    int32_t *scratch;                 /* a pull's frames before the device's format */
    size_t scratch_cap;
    size_t tmp_cap;
    PcWav dump;
    PcAudioStats stats;
    double clock_last, clock_carry;   /* null backend */
    int32_t gain;                     /* the master volume, Q16 (pc_audio_set_volume) */
    double last_push, first_push;     /* when the game last and first pushed (clock_now) */
    int pushed;                       /* the game has pushed since the output opened */
    int dry;                          /* the last pull or drain ran out */
    uint32_t target;                  /* game frames the AI keeps queued (pc_audio_target_frames) */
    uint32_t min_ms;                  /* pc_audio_set_latency */
    int32_t last[2];                  /* the last frame played: an underrun's fade starts there */
    uint32_t fade_in;                 /* frames of fade-in left after an underrun */
    void (*pull_hook)(void);
    uint32_t reported;                /* underruns logged so far */
    int paused;                       /* pc_audio_set_paused: the game is held, its silence isn't an underrun */
    double t0;                        /* when the output opened (clock_now) */
    double underrun_at;               /* when the last underrun began, from t0 */
    /* the game's AI buffers and the pushes held for their renders */
    Buf bufs[RING_MAX];
    uint64_t ops, pushes;             /* uses of the table; pushes made */
    uint64_t ring_len;                /* the game's ring of AI buffers, in pushes (0 until a buffer comes again) */
    int anchored;                     /* renders are matched to pushes (a render came since ring_len is known) */
    int rendered_any;                 /* a render has come since the output opened */
    uint64_t last_render;             /* the push the last render was for */
    const int16_t *last_render_buf;
    uint64_t last_heard;              /* the push whose render last went to the converter: what plays keeps order */
    uint64_t pushes_at_render;        /* pushes made at the last render */
    double last_render_t;             /* when it came (clock_now) */
    Held held[HOLD_MAX];
    unsigned held_first, held_n;
    size_t held_frames;               /* game frames held */
    int16_t last_in[2];               /* the last frame into the converter: a substituted silence fades from it */
    uint32_t in_fade;                 /* frames into the converter to fade in, after a substituted silence */
    /* the periodic report (pc_audio_set_report_every) */
    unsigned report_every;
    double report_at;
    PcAudioStats reported_stats;
    uint32_t span_min, span_max;      /* the queue's least and most since the last periodic report */
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
        g.ring = malloc(cap * 8);
        g.cap = g.ring ? cap : 0;
    }
    g.rd = g.count = 0;
}

static void ring_write(const int32_t *s, size_t n)
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
    memcpy(g.ring + 2 * wr, s, first * 8);
    memcpy(g.ring, s + 2 * first, (n - first) * 8);
    g.count += n;
}

static size_t ring_read(int32_t *out, size_t n)
{
    size_t got = n < g.count ? n : g.count;
    size_t first = g.cap - g.rd < got ? g.cap - g.rd : got;
    if (got) {
        memcpy(out, g.ring + 2 * g.rd, first * 8);
        memcpy(out + 2 * first, g.ring, (got - first) * 8);
        g.rd = (g.rd + got) % g.cap;
        g.count -= got;
    }
    return got;
}

static uint32_t game_frames(uint64_t device_frames)
{
    return g.dev_rate && g.src_rate ? (uint32_t)(device_frames * g.src_rate / g.dev_rate) : (uint32_t)device_frames;
}

static uint32_t device_frames(uint64_t game)
{
    return g.dev_rate && g.src_rate ? (uint32_t)(game * g.dev_rate / g.src_rate) : (uint32_t)game;
}

/* ---------------------------------------------------------------- paced mode's tap (docs/port/AUDIO.md 11)
 *
 * In paced mode (pc_audio_set_paced) the queue above is the null output's whatever the backend: it drains by the clock
 * pc_audio_set_clock gives (the N64's), so the AI's events, the holds for renders and what the converter gets are
 * --fast's with the null output, to the bit. A device gets a copy: each frame the converter gets (after the holds'
 * matching, so the dump and the device hear the same) also goes, through a converter of the tap's own, into a FIFO the
 * device drains. The FIFO keeps about 50 ms (audio.latency raises it); the paced N64 clock and the sound card's crystal
 * drift apart, so its converter's ratio is nudged to hold that level: a fraction of a percent for the level's error,
 * and the clocks' steady drift integrated (a sound card's: thousandths of a percent; at most 5%). When the
 * FIFO runs dry the device plays silence (faded, as an underrun) until it has its level again: a click on this PC,
 * never anything the game sees. With the lock held, as the rest. */
#define TAP_TARGET_MS 50u      /* the FIFO's level */
#define TAP_MAX_MS 250u        /* past this (the device stalled) the oldest frames go, down to the level */
#define TAP_GAIN_P 0.02        /* the nudge for the level's relative error (smoothed): 10% off (5 ms), 0.2% */
#define TAP_P_MAX 0.005        /* ... at most 0.5% */
#define TAP_GAIN_I 0.012       /* and for its integral (a second at 10% off: 0.12%): the clocks' steady drift */
#define TAP_I_MAX 0.05         /* ... at most 5%: a sound card's crystal is thousandths of a percent off the wall clock,
                                * so the nudge stays a fraction of a percent; SDL's disk driver (the tests' device)
                                * consumes 1-3% slow, as SDL_Delay oversleeps each of its 10 ms periods */
#define TAP_SMOOTH_S 0.5       /* the level's smoothing */

typedef struct {
    uint64_t pulls, underruns, underrun_frames, priming_frames, idle_frames, dropped, overflows, trimmed;
    uint64_t level_sum, level_count;
    uint32_t level_min, level_max;
    double nudge_sum, nudge_min, nudge_max;
} TapStats;

static struct {
    int want;                  /* pc_audio_set_paced */
    int on;                    /* a device opened as the tap */
    int held;                  /* pc_audio_tap_hold: a rejoin's replay, nothing to the device */
    const PcAudioBackendOps *dev;
    PcAudioDevice device;
    uint32_t rate;             /* the device's */
    uint32_t period;           /* its largest pull */
    PcResampler rs;            /* the game's rate to the device's, nudged */
    int32_t *ring;             /* the FIFO, device frames */
    size_t cap, rd, count;
    int32_t *tmp;
    size_t tmp_cap;
    uint32_t target;           /* the FIFO's level, device frames */
    int playing;               /* the FIFO reached its level since the start, or since it last ran dry */
    int dry;                   /* it ran dry while the game played, and hasn't its level back yet */
    uint32_t span_min, span_max;          /* the level's least and most since the last periodic line */
    double span_nmin, span_nmax;          /* the nudge's */
    double level;              /* the FIFO at the pulls, smoothed */
    double integral, nudge;
    double warm;               /* seconds of the level measured since the FIFO began to play (again) */
    int32_t last[2];           /* the last frame played: a dry FIFO's fade starts there */
    uint32_t fade_in;
    double last_feed;          /* when the game's sound last came (now_seconds) */
    double opened;             /* when the device opened (now_seconds) */
    TapStats stats, reported;  /* in all; at the last periodic line */
} T;

static void tap_free(void)
{
    free(T.ring);
    free(T.tmp);
    T.ring = T.tmp = NULL;
    T.cap = T.tmp_cap = T.rd = T.count = 0;
}

/* The game's rate is known: the tap's converter, at its nominal ratio until the level nudges it. */
static void tap_configure(uint32_t src)
{
    if (!T.on)
        return;
    rs_init_nudged(&T.rs, src, T.rate);
    rs_nudge(&T.rs, T.nudge);
    size_t need_out = rs_max_out(&T.rs, RS_CHUNK);
    if (need_out > T.tmp_cap) {
        free(T.tmp);
        T.tmp = malloc(need_out * 8);
        T.tmp_cap = T.tmp ? need_out : 0;
    }
    if (!T.ring) {
        T.cap = T.rate;   /* a second */
        T.ring = malloc(T.cap * 8);
        if (!T.ring)
            T.cap = 0;
        T.rd = T.count = 0;
    }
}

static void tap_retarget(void)
{
    uint32_t ms = g.min_ms > TAP_TARGET_MS ? g.min_ms : TAP_TARGET_MS;
    T.target = T.rate * ms / 1000;
}

/* What the converter got, n game frames: into the FIFO. */
static void tap_feed(const int16_t *s, size_t n)
{
    if (!T.on || T.held || !T.tmp || !T.cap || !n)
        return;
    T.last_feed = now_seconds();
    while (n) {
        size_t k = n > RS_CHUNK ? RS_CHUNK : n;
        size_t made = rs_process32(&T.rs, s, k, T.tmp);
        if (made > T.cap)
            made = T.cap;
        if (T.count + made > T.cap) {   /* a second queued: the device has stopped */
            size_t drop = T.count + made - T.cap;
            T.rd = (T.rd + drop) % T.cap;
            T.count -= drop;
            T.stats.dropped += drop;
        }
        size_t wr = (T.rd + T.count) % T.cap, first = T.cap - wr < made ? T.cap - wr : made;
        memcpy(T.ring + 2 * wr, T.tmp, first * 8);
        memcpy(T.ring, T.tmp + 2 * first, (made - first) * 8);
        T.count += made;
        s += 2 * k;
        n -= k;
    }
    if (!T.playing && T.count > T.target) {   /* the device hasn't begun (or begun again): the newest at the level */
        size_t trim = T.count - T.target;
        T.rd = (T.rd + trim) % T.cap;
        T.count -= trim;
        T.stats.trimmed += trim;
        return;
    }
    uint32_t max = T.rate * (TAP_MAX_MS > g.min_ms * 2 ? TAP_MAX_MS : g.min_ms * 2) / 1000;
    if (T.count > max) {   /* the device stalled: the oldest go, down to the level */
        size_t drop = T.count - T.target;
        T.rd = (T.rd + drop) % T.cap;
        T.count -= drop;
        T.stats.dropped += drop;
        if (T.stats.overflows++ < REPORT_MAX)
            fprintf(stderr, "audio: tap: the FIFO past %u ms, %.1f s in (the device %s): %.0f ms dropped\n",
                    (unsigned)(max * 1000u / T.rate), T.last_feed - T.opened,
                    T.playing ? "pulled too little" : "hasn't pulled since it filled", drop * 1000.0 / T.rate);
    }
}

/* The level at a pull of n frames, while the FIFO plays the game's sound: counted, and the nudge follows it. */
static void tap_level(size_t n)
{
    uint32_t level = (uint32_t)T.count;
    TapStats *s = &T.stats;
    if (!s->level_count || level < s->level_min)
        s->level_min = level;
    if (level > s->level_max)
        s->level_max = level;
    s->level_sum += level;
    s->level_count++;
    if (!T.target || !T.rate)
        return;
    double dt = (double)n / T.rate, a = dt / TAP_SMOOTH_S;
    T.level = T.level <= 0 ? level : T.level + ((double)level - T.level) * (a < 1 ? a : 1);
    double e = (T.level - T.target) / T.target;   /* > 0: too full, the device hears too much: make less */
    double p = TAP_GAIN_P * e;
    if (T.warm < TAP_SMOOTH_S)   /* the smoothed level settles first: the drift's integral doesn't wind up on it */
        T.warm += dt;
    else
        T.integral += TAP_GAIN_I * e * dt;
    if (T.integral > TAP_I_MAX)
        T.integral = TAP_I_MAX;
    if (T.integral < -TAP_I_MAX)
        T.integral = -TAP_I_MAX;
    if (p > TAP_P_MAX)
        p = TAP_P_MAX;
    if (p < -TAP_P_MAX)
        p = -TAP_P_MAX;
    T.nudge = -(p + T.integral);   /* within RS_NUDGE_MAX */
    rs_nudge(&T.rs, T.nudge);
    s->nudge_sum += T.nudge;
    if (s->level_count == 1 || T.nudge < s->nudge_min)
        s->nudge_min = T.nudge;
    if (s->level_count == 1 || T.nudge > s->nudge_max)
        s->nudge_max = T.nudge;
    int first = s->level_count - T.reported.level_count == 1;   /* the span's first */
    if (first || level < T.span_min)
        T.span_min = level;
    if (first || level > T.span_max)
        T.span_max = level;
    if (first || T.nudge < T.span_nmin)
        T.span_nmin = T.nudge;
    if (first || T.nudge > T.span_nmax)
        T.span_nmax = T.nudge;
}

/* A device's pull in paced mode, into g.scratch (int32, at the volume). Returns the frames the FIFO had; *log_it says
 * a dry FIFO the log should name. Never touches the queue the AI keeps. */
static size_t tap_pull_locked(size_t frames, int *log_it)
{
    int32_t *out = g.scratch;
    double now = now_seconds();
    int game = g.pushed && !g.paused && now - T.last_feed < PLAYING_S;   /* the game plays (not held, not quiet) */
    size_t got = 0;
    if (!T.playing && T.count >= T.target && T.count) {   /* the level reached: it plays (again) */
        T.playing = 1;
        T.dry = 0;
        T.fade_in = FADE;
        T.level = 0;
        T.warm = 0;
    }
    if (T.playing && T.cap) {
        if (game)
            tap_level(frames);
        got = frames < T.count ? frames : T.count;
        size_t first = T.cap - T.rd < got ? T.cap - T.rd : got;
        if (got) {
            memcpy(out, T.ring + 2 * T.rd, first * 8);
            memcpy(out + 2 * first, T.ring, (got - first) * 8);
            T.rd = (T.rd + got) % T.cap;
            T.count -= got;
        }
    }
    if (got && T.fade_in) {   /* from silence */
        uint32_t n = got < T.fade_in ? (uint32_t)got : T.fade_in;
        for (uint32_t i = 0; i < n; i++) {
            int32_t k = (int32_t)(FADE - T.fade_in + i + 1);
            out[2 * i] = out[2 * i] * k / (int32_t)FADE;
            out[2 * i + 1] = out[2 * i + 1] * k / (int32_t)FADE;
        }
        T.fade_in -= n;
    }
    if (got) {
        T.last[0] = out[2 * got - 2];
        T.last[1] = out[2 * got - 1];
    }
    if (got < frames) {
        size_t n = frames - got, f = n < FADE ? n : FADE;
        int32_t *z = out + 2 * got;
        if (T.playing && game) {   /* ran dry while the game plays: an underrun, until the level is back */
            T.stats.underruns++;
            T.dry = 1;
            if (g.reported < REPORT_MAX) {
                *log_it = 2;
                g.reported++;
                g.underrun_at = now - T.opened;
            }
        }
        if (!game)
            T.stats.idle_frames += n;
        else if (T.dry)
            T.stats.underrun_frames += n;
        else
            T.stats.priming_frames += n;   /* filling to its level: at the start, after the game was held */
        T.playing = 0;
        for (size_t i = 0; i < f; i++) {   /* the last frame fades to silence: a step to zero would click */
            int32_t k = (int32_t)(FADE - 1 - i);
            z[2 * i] = T.last[0] * k / (int32_t)FADE;
            z[2 * i + 1] = T.last[1] * k / (int32_t)FADE;
        }
        memset(z + 2 * f, 0, (n - f) * 8);
        T.last[0] = T.last[1] = 0;
    }
    T.stats.pulls++;
    if (frames > T.period && frames < T.rate)   /* the tap's own: g.stats.period_frames would move the AI's target */
        T.period = (uint32_t)frames;
    if (g.gain != 65536)
        for (size_t i = 0; i < 2 * frames; i++)
            out[i] = (int32_t)((int64_t)out[i] * g.gain >> 16);
    return got;
}

/* The tap's line: since *from (NULL: since it opened); span names that time for a periodic line. */
static void tap_report(const char *span, const TapStats *from)
{
    TapStats z, d = T.stats;
    const TapStats *f = from ? from : (memset(&z, 0, sizeof z), &z);
    double rate = T.rate ? T.rate : 1, src = g.src_rate ? g.src_rate : DEFAULT_RATE;
    d.pulls -= f->pulls, d.underruns -= f->underruns, d.underrun_frames -= f->underrun_frames;
    d.priming_frames -= f->priming_frames, d.idle_frames -= f->idle_frames, d.dropped -= f->dropped;
    d.trimmed -= f->trimmed;
    d.level_sum -= f->level_sum, d.level_count -= f->level_count, d.nudge_sum -= f->nudge_sum;
    char level[200] = "the FIFO not measured";
    if (d.level_count)
        snprintf(level, sizeof level, "the FIFO %.1f/%.1f/%.1f ms (least/mean/most) against %.0f ms; the ratio nudged "
                 "%+.4f%% on average (%+.4f%% to %+.4f%%), %+.4f%% now", (from ? T.span_min : d.level_min) * 1000.0 / rate,
                 (double)d.level_sum / (double)d.level_count * 1000.0 / rate,
                 (from ? T.span_max : d.level_max) * 1000.0 / rate, T.target * 1000.0 / rate,
                 100.0 * d.nudge_sum / (double)d.level_count, 100.0 * (from ? T.span_nmin : d.nudge_min),
                 100.0 * (from ? T.span_nmax : d.nudge_max), 100.0 * T.nudge);
    fprintf(stderr, "audio: tap to %s (%s) at %u Hz%s, pulls of %u frames: %u Hz converted x%.5f; %s; %llu underruns "
            "(%.0f ms of silence), %.0f ms filling, %.0f ms idle, %llu frames dropped; %.0f ms trimmed before the device "
            "played\n", T.dev ? T.dev->name : "?",
            T.device.driver, (unsigned)T.rate, span, (unsigned)T.period, (unsigned)src, T.rate / src,
            level, (unsigned long long)d.underruns, d.underrun_frames * 1000.0 / rate, d.priming_frames * 1000.0 / rate,
            d.idle_frames * 1000.0 / rate, (unsigned long long)d.dropped, d.trimmed * 1000.0 / rate);
}

void pc_audio_set_paced(int on)
{
    lock();
    T.want = on != 0;
    unlock();
}

void pc_audio_tap_hold(int on)
{
    lock();
    T.held = on != 0;
    if (T.held) {   /* what the FIFO has is from before: the device plays silence until it fills again */
        T.rd = T.count = 0;
        T.playing = 0;
    }
    unlock();
}

/* n game frames into the converter, the ring and the dump (and paced mode's tap). */
static void convert(const int16_t *s, size_t n)
{
    if (!n)
        return;
    pc_wav_write(&g.dump, s, n);
    tap_feed(s, n);
    g.last_in[0] = s[2 * n - 2];
    g.last_in[1] = s[2 * n - 1];
    while (n && g.tmp) {
        size_t k = n > RS_CHUNK ? RS_CHUNK : n;
        size_t made = rs_process32(&g.rs, s, k, g.tmp);
        ring_write(g.tmp, made);
        s += 2 * k;
        n -= k;
    }
}

/* The game's sound: after a substituted silence it fades in. */
static void feed(const int16_t *s, size_t n)
{
    if (g.in_fade && n) {
        int16_t head[2 * FADE];
        size_t k = n < g.in_fade ? n : g.in_fade;
        for (size_t i = 0; i < k; i++) {
            int32_t m = (int32_t)(FADE - g.in_fade + i + 1);
            head[2 * i] = (int16_t)(s[2 * i] * m / (int32_t)FADE);
            head[2 * i + 1] = (int16_t)(s[2 * i + 1] * m / (int32_t)FADE);
        }
        g.in_fade -= (uint32_t)k;
        convert(head, k);
        s += 2 * k;
        n -= k;
    }
    convert(s, n);
}

/* Silence in place of a buffer: the last frame fades out over 32 frames, as an underrun's does. */
static void feed_silence(size_t n)
{
    int16_t z[2 * 256];
    int32_t l = g.last_in[0], r = g.last_in[1];
    size_t done = 0;
    while (done < n) {
        size_t k = n - done > 256 ? 256 : n - done;
        for (size_t i = 0; i < k; i++) {
            int32_t m = done + i < FADE ? (int32_t)(FADE - 1 - (done + i)) : 0;
            z[2 * i] = (int16_t)(l * m / (int32_t)FADE);
            z[2 * i + 1] = (int16_t)(r * m / (int32_t)FADE);
        }
        convert(z, k);
        done += k;
    }
    g.in_fade = FADE;
}

static Held *held_at(unsigned i) { return &g.held[(g.held_first + i) % HOLD_MAX]; }

/* Held pushes go to the converter in order, up to the first still waiting for its render. */
static void held_drain(void)
{
    while (g.held_n) {
        Held *h = held_at(0);
        if (h->state == H_WAITING)
            break;
        if (h->state == H_READY) {
            feed(h->data, h->frames);
            g.last_heard = h->sound;
        } else
            feed_silence(h->frames);
        g.held_frames -= h->frames;
        g.held_first = (g.held_first + 1) % HOLD_MAX;
        g.held_n--;
    }
}

static Buf *buf_of(const int16_t *buf)
{
    Buf *at = &g.bufs[0];
    g.ops++;
    for (int i = 0; i < RING_MAX; i++) {
        if (g.bufs[i].buf == buf) {
            g.bufs[i].op = g.ops;
            return &g.bufs[i];
        }
        if (g.bufs[i].op < at->op)
            at = &g.bufs[i];
    }
    memset(at, 0, sizeof *at);
    at->buf = buf;
    at->op = g.ops;
    return at;
}

static int all_zero(const int16_t *s, size_t n)
{
    for (size_t i = 0; i < 2 * n; i++)
        if (s[i])
            return 0;
    return 1;
}

/* The device needs the oldest held push and its render hasn't come: the buffer's previous render if that never
 * played and comes after what last did (the renderer is a turn behind: its sound plays in order, late), else
 * silence. */
static void give_up(void)
{
    Held *h = held_at(0);
    Buf *b = buf_of(h->buf);
    if (b->render && b->render < h->index && b->render > g.last_heard) {
        memcpy(h->data, h->buf, h->frames * 4);
        h->sound = b->render;
        h->state = H_READY;
        b->render = 0;
        g.stats.played_late++;
    } else {
        h->state = H_SILENT;
        g.stats.silenced++;
        g.stats.missed += !all_zero(h->buf, h->frames);
    }
}

/* Make sure the ring has n device frames, if held pushes can give them: those waiting give up. */
static void need(size_t n)
{
    while (g.count < n && g.held_n) {
        if (held_at(0)->state == H_WAITING)
            give_up();
        held_drain();
    }
}

/* Everything held goes to the converter now: a push that can't be held comes after it. */
static void flush_held(void)
{
    while (g.held_n) {
        if (held_at(0)->state == H_WAITING)
            give_up();
        held_drain();
    }
}

/* The AI's target: from the device's pulls when it has them, at least the latency asked for, never below what it
 * has grown to. */
static void retarget(void)
{
    uint32_t rate = g.src_rate ? g.src_rate : DEFAULT_RATE;
    uint32_t max = rate * TARGET_MAX_MS / 1000 > TARGET_MIN ? rate * TARGET_MAX_MS / 1000 : TARGET_MIN;
    uint32_t t = TARGET_MIN;
    if (g.stats.period_frames && !T.want) {   /* paced mode: the null output's target, whatever pulls (the tap's) */
        t = game_frames(2 * (uint64_t)g.stats.period_frames) + rate / 60 + TARGET_STEP;
        if (game_frames(3 * (uint64_t)g.stats.period_frames) > max)   /* a device with long periods */
            max = game_frames(3 * (uint64_t)g.stats.period_frames);
    }
    if (rate * g.min_ms / 1000 > t && !T.want)   /* paced mode: audio.latency is the tap's FIFO (per PC) */
        t = rate * g.min_ms / 1000;
    if (t > g.target)
        g.target = t;
    if (g.target > max)
        g.target = max;
    g.stats.target_frames = g.target;
    tap_retarget();
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

void pc_audio_set_clock(double (*now)(void))
{
    lock();
    g_clock = now;
    g.clock_last = g.clock_carry = 0;
    g.t0 = g.report_at = clock_now();
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
        need(n);
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
    if (T.want && g.kind == KIND_NULL)   /* paced mode's queue: the null output's at the game's rate, with a device
                                          * or not; RUSH2049_AUDIO_RATE is the tap's device's */
        g.dev_rate = src;
    else if (g.kind != KIND_DEVICE)
        g.dev_rate = g.forced_rate ? g.forced_rate : src;
    tap_configure(src);
    rs_init(&g.rs, g.src_rate, g.dev_rate);
    size_t need_out = rs_max_out(&g.rs, RS_CHUNK);
    if (need_out > g.tmp_cap) {
        free(g.tmp);
        g.tmp = malloc(need_out * 8);
        g.tmp_cap = g.tmp ? need_out : 0;
    }
    if (g.cap < g.dev_rate || !g.ring)
        ring_reset(g.dev_rate);
    pc_wav_set_rate(&g.dump, src);
    g.stats.device_rate = g.dev_rate;
    g.stats.game_rate = src;
    retarget();
}

/* The mixer's line (pc_audio_mixer_stats, docs/port/AUDIO.md 10): the subframes rendered since the last periodic
 * report (since: 1) or in all, the voices they mixed, and the interpreter's CPU time a subframe, against the
 * subframe's own length (192 frames at the game's rate). Nothing before the first task. */
static void mixer_report(const char *span, int since)
{
    static PcMixerStats last;
    PcMixerStats m, d;
    pc_audio_mixer_stats(&m);
    d = m;
    if (since) {
        d.subframes -= last.subframes, d.voices -= last.voices, d.ns -= last.ns;
        last = m;
    }
    if (!d.subframes)
        return;
    double src = g.src_rate ? g.src_rate : DEFAULT_RATE, us = d.ns / 1000.0 / (double)d.subframes;
    fprintf(stderr, "audio: mixer%s: %llu subframes of %u voice records; %.2f voices a subframe (most %u); %.1f us "
            "a subframe of CPU (%.2f%% of its %.2f ms), most %.0f us\n", span, (unsigned long long)d.subframes,
            m.slots, (double)d.voices / (double)d.subframes, m.voices_most, us, us / (1e4 * MX_SUBFRAME / src),
            1000.0 * MX_SUBFRAME / src, m.ns_most / 1000.0);
}

/* The counters since *from (NULL: since the output opened), in one line; span names that time for a periodic line,
 * lmin and lmax are the queue's least and most over it. */
static void report(const char *span, const PcAudioStats *from, uint32_t lmin, uint32_t lmax)
{
    PcAudioStats z, d = g.stats;
    const PcAudioStats *f = from ? from : (memset(&z, 0, sizeof z), &z);
    double rate = g.dev_rate ? g.dev_rate : 1, src = g.src_rate ? g.src_rate : DEFAULT_RATE;
    d.underruns -= f->underruns, d.underrun_frames -= f->underrun_frames, d.idle_frames -= f->idle_frames;
    d.waited -= f->waited, d.rescued -= f->rescued, d.played_late -= f->played_late, d.silenced -= f->silenced;
    d.missed -= f->missed, d.skipped -= f->skipped, d.rematched -= f->rematched, d.clipped -= f->clipped;
    d.renderer_skips -= f->renderer_skips;
    d.dropped_frames -= f->dropped_frames, d.pushed_frames -= f->pushed_frames, d.pulls -= f->pulls;
    d.level_sum -= f->level_sum, d.level_count -= f->level_count;
    double mean = d.level_count ? (double)d.level_sum / (double)d.level_count : 0;
    char device[96] = "", queue[96] = "queue not measured";
    if (g.kind == KIND_DEVICE)
        snprintf(device, sizeof device, "%s, pulls of %u frames; ", g.device.driver, (unsigned)d.period_frames);
    if (d.level_count)
        snprintf(queue, sizeof queue, "queue %.0f/%.0f/%.0f ms (least/mean/most)", lmin * 1000.0 / rate,
                 mean * 1000.0 / rate, lmax * 1000.0 / rate);
    fprintf(stderr,
            "audio: %s at %u Hz (%s%u Hz converted x%.5f)%s: %.1f s played; %llu underruns (%.0f ms of silence); %s, "
            "target %u frames (%.0f ms); %llu buffers pushed before their render: %llu rendered in time, %llu played "
            "late, %llu as silence (%llu replacing sound); %llu renders not played, %llu rematches, %llu buffers the "
            "renderer skipped; %llu samples clipped, %llu frames dropped, %.0f ms idle\n",
            g.name, (unsigned)g.dev_rate, device, (unsigned)src, g.dev_rate / src, span, d.pushed_frames / src,
            (unsigned long long)d.underruns, d.underrun_frames * 1000.0 / rate, queue, (unsigned)d.target_frames,
            d.target_frames * 1000.0 / src, (unsigned long long)d.waited, (unsigned long long)d.rescued,
            (unsigned long long)d.played_late, (unsigned long long)d.silenced, (unsigned long long)d.missed,
            (unsigned long long)d.skipped, (unsigned long long)d.rematched, (unsigned long long)d.renderer_skips,
            (unsigned long long)d.clipped,
            (unsigned long long)d.dropped_frames, d.idle_frames * 1000.0 / rate);
    mixer_report(span, from != NULL);
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

static void reset_tracking(void)
{
    memset(g.bufs, 0, sizeof g.bufs);
    g.ops = g.pushes = g.ring_len = g.last_render = g.pushes_at_render = 0;
    g.anchored = g.rendered_any = 0;
    g.last_render_buf = NULL;
    g.last_heard = 0;
    g.held_first = g.held_n = 0;
    g.held_frames = 0;
    g.last_in[0] = g.last_in[1] = 0;
    g.in_fade = 0;
}

int pc_audio_init(const char *backend, const char *dump_wav_path)
{
    const char *want = backend && *backend ? backend : "auto";
    const char *env_rate = getenv("RUSH2049_AUDIO_RATE"), *env_stats = getenv("RUSH2049_AUDIO_STATS");
    int rc = 0;
    uint32_t keep_rate = g.src_rate;

    if (g.inited)
        pc_audio_shutdown();

    lock();
    g.inited = 1;
    g.closed = 0;
    g.src_rate = keep_rate;
    g.kind = KIND_NULL;
    g.dev = NULL;
    memset(&g.device, 0, sizeof g.device);
    g.forced_rate = env_rate ? (uint32_t)strtoul(env_rate, NULL, 10) : 0;
    memset(&g.stats, 0, sizeof g.stats);
    memset(&g.reported_stats, 0, sizeof g.reported_stats);
    g.clock_last = g.clock_carry = 0;
    g.pushed = g.dry = 0;
    g.first_push = 0;
    g.target = TARGET_MIN;
    g.last[0] = g.last[1] = 0;
    g.fade_in = 0;
    g.reported = 0;
    g.t0 = g.report_at = clock_now();
    if (env_stats)
        g.report_every = (unsigned)strtoul(env_stats, NULL, 10);
    reset_tracking();
    {   /* paced mode's tap: closed (pc_audio_shutdown), and nothing kept from before */
        int want = T.want;
        tap_free();
        memset(&T, 0, sizeof T);
        T.want = want;
    }
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

    /* A device opens at its own rate unless one is forced; the queue converts the game's to it. */
    uint32_t want_rate = g.forced_rate;
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
        PcAudioDevice dev;
        memset(&dev, 0, sizeof dev);
        if (ops && ops->open(want_rate, &dev) == 0 && dev.rate) {
            lock();
            if (T.want) {   /* paced mode: the device is the tap's; the queue stays the null output's */
                T.on = 1;
                T.dev = ops;
                T.device = dev;
                T.rate = dev.rate;
                T.opened = now_seconds();
            } else {
                g.kind = KIND_DEVICE;
                g.dev = ops;
                g.device = dev;
                g.dev_rate = dev.rate;
            }
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
             g.kind == KIND_DEVICE ? g.dev->name : g.kind == KIND_MANUAL ? "manual" : T.on ? "paced" :
             g.dump.f ? "wav" : "null");
    configure(g.src_rate ? g.src_rate : DEFAULT_RATE);
    if (T.on)
        fprintf(stderr, "audio: paced: the queue the null output's on the N64 clock; a tap to %s (%s) at %u Hz, pulls "
                "of %u frames, the FIFO %u ms%s%s\n", T.dev->name, T.device.driver, (unsigned)T.rate,
                (unsigned)T.device.period, (unsigned)(T.target * 1000u / (T.rate ? T.rate : 1)),
                g.dump.f ? ", dumping to " : "", g.dump.f ? dump_wav_path : "");
    else if (g.kind == KIND_DEVICE)
        fprintf(stderr, "audio: output %s (%s) at %u Hz, pulls of %u frames%s%s\n", g.name, g.device.driver,
                (unsigned)g.dev_rate, (unsigned)g.device.period, g.dump.f ? ", dumping to " : "",
                g.dump.f ? dump_wav_path : "");
    else
        fprintf(stderr, "audio: output %s at %u Hz%s%s\n", g.name, (unsigned)g.dev_rate,
                g.dump.f ? ", dumping to " : "", g.dump.f ? dump_wav_path : "");
    unlock();
    return rc;
}

/* 0 once the output is shut down. The game's threads still run while main shuts it down at exit, and a push then
 * opened it again (with the old unlocked test, a data race ThreadSanitizer found: docs/port/SANITIZE.md). */
static int ensure_init(void)
{
    lock();
    int inited = g.inited, closed = g.closed;
    unlock();
    if (closed)
        return 0;
    if (!inited)
        pc_audio_init(getenv("RUSH2049_AUDIO"), getenv("RUSH2049_DUMP_AUDIO"));
    return 1;
}

void pc_audio_shutdown(void)
{
    const PcAudioBackendOps *dev;
    lock();
    g.closed = 1;
    if (!g.inited) {
        unlock();
        return;
    }
    if (g.stats.pushed_frames) {
        report("", NULL, g.stats.level_min, g.stats.level_max);
        if (T.on)
            tap_report("", NULL);
    }
    dev = g.kind == KIND_DEVICE ? g.dev : T.on ? T.dev : NULL;
    g.kind = KIND_NULL;
    g.dev = NULL;
    unlock();
    if (dev)
        dev->close();   /* the tap's device pulls its FIFO until this returns */
    lock();
    T.on = 0;
    tap_free();
    pc_wav_close(&g.dump);
    free(g.ring);
    free(g.tmp);
    free(g.scratch);
    g.ring = g.tmp = g.scratch = NULL;
    g.cap = g.tmp_cap = g.scratch_cap = g.rd = g.count = 0;
    g.src_rate = g.dev_rate = 0;
    reset_tracking();
    g.inited = 0;
    unlock();
}

/* ---------------------------------------------------------------- the game's side */

void pc_audio_set_frequency(uint32_t hz)
{
    if (!ensure_init() || !hz)
        return;
    lock();
    if (g.closed) {
        unlock();
        return;
    }
    if (hz != g.src_rate) {
        flush_held();
        configure(hz);
        if (g.kind != KIND_DEVICE)
            ring_reset(g.dev_rate);   /* queued frames were at the old rate */
    }
    unlock();
}

void pc_audio_push(const int16_t *samples, size_t stereo_frames)
{
    if (!ensure_init())
        return;
    lock();
    if (g.closed) {
        unlock();
        return;
    }
    if (!g.src_rate)
        configure(DEFAULT_RATE);
    null_drain();
    g.stats.pushed_frames += stereo_frames;
    if (stereo_frames) {
        g.pushed = 1;
        g.last_push = clock_now();
        if (!g.first_push)
            g.first_push = g.last_push;
        uint64_t index = ++g.pushes;
        Buf *b = buf_of(samples);
        int again = b->pushed != 0, fresh = b->fresh, hold = stereo_frames <= HOLD_FRAMES;
        if (again) {   /* the buffer comes round again: the ring's length */
            uint64_t len = index - b->pushed;
            if (len != g.ring_len)
                g.anchored = 0;   /* a new ring: renders are matched again from the next */
            g.ring_len = len;
        }
        b->pushed = index;
        b->fresh = 0;
        /* Matched: wait for this push's render. Not yet (the game's first turns): a buffer that comes again without
         * a render since is the ring's last turn, and plays as silence. */
        int wait = hold && g.anchored && b->render != index;
        int stale = hold && !g.anchored && g.rendered_any && again && !fresh;
        if (b->render == index)
            b->render = 0;   /* its render plays now */
        if (stale) {
            g.stats.silenced++;
            g.stats.missed += !all_zero(samples, stereo_frames);
        }
        if (!hold) {
            flush_held();
            feed(samples, stereo_frames);
            g.last_heard = index;
        } else if (!wait && !stale && !g.held_n) {
            feed(samples, stereo_frames);
            g.last_heard = index;
        } else {
            if (g.held_n == HOLD_MAX) {
                if (held_at(0)->state == H_WAITING)
                    give_up();
                held_drain();
            }
            Held *h = held_at(g.held_n);
            h->buf = samples;
            h->index = h->sound = index;
            h->frames = (uint32_t)stereo_frames;
            h->state = wait ? H_WAITING : stale ? H_SILENT : H_READY;
            if (h->state == H_READY)
                memcpy(h->data, samples, stereo_frames * 4);
            g.stats.waited += (uint64_t)(wait || stale);
            g.held_n++;
            g.held_frames += stereo_frames;
            held_drain();
        }
    }
    if (g.count > g.stats.max_queued)
        g.stats.max_queued = (uint32_t)g.count;
    unlock();
}

size_t pc_audio_queued_frames(void)
{
    size_t n;
    if (!ensure_init())
        return 0;
    lock();
    null_drain();
    n = game_frames(g.count) + g.held_frames;
    unlock();
    return n;
}

/* The interpreter wrote one of the game's AI buffers (musyx.c). It is for the push after the last render's: the game
 * renders its ring in order. Unless the renderer may have started over: the order jumps, or a ring turn of pushes
 * went by without a render. Then it is for the buffer's next push, as at the ring's front. The game stops its
 * renderer after it has rendered 8 more buffers (func_80014140), at the front unless it was behind, and starts it
 * again at the new front (func_800140F8): from the front, a jump as long as the pushes made meanwhile, which only a
 * whole turn of them hides. Matched as ahead, a renderer that had only stalled plays a turn late, in order, never
 * early; when it catches up, the match would pass the ring's front and comes back. A match that moves drops the
 * renders kept for pushes from there on: under the old one they would play out of order. The same buffer rendered
 * twice running (the game does it at loads) is the same push's, the later render in place of the earlier. A held
 * push the render is for plays now. */
void pc_audio_note_rendered(const int16_t *buf)
{
    lock();
    Buf *b = buf_of(buf);
    uint64_t len = g.ring_len;
    b->fresh = 1;
    g.rendered_any = 1;
    if (!len || !b->pushed) {   /* nothing to match it to yet: the next pushes play as they come */
        g.anchored = 0;
        unlock();
        return;
    }
    const Buf *prev = NULL;
    for (int i = 0; i < RING_MAX && g.last_render_buf; i++)
        if (g.bufs[i].buf == g.last_render_buf && g.bufs[i].pushed)
            prev = &g.bufs[i];
    int again = g.anchored && len > 1 && prev == b && g.pushes - g.pushes_at_render < len;   /* rendered again */
    double now = clock_now();
    int follows = g.anchored && prev && (prev->pushed + 1) % len == b->pushed % len &&
                  g.pushes - g.pushes_at_render < len;
    uint64_t estimate = !g.anchored ? 0 : again ? g.last_render : g.last_render + 1;
    uint64_t index = follows || again ? estimate : b->pushed + len;
    if (index > b->pushed + len)   /* further ahead than the ring allows: the match had slipped a turn */
        index = b->pushed + len;
    if (estimate && index != estimate) {
        g.stats.rematched++;
        for (int i = 0; i < RING_MAX; i++)
            if (g.bufs[i].render >= index) {
                g.stats.skipped++;
                g.bufs[i].render = 0;
            }
    }
    g.anchored = 1;
    g.last_render = index;
    g.last_render_buf = buf;
    g.pushes_at_render = g.pushes;
    /* Further on in the ring right after the last render: the renderer passed buffers over (docs/port/AUDIO.md 7). */
    if (estimate && !follows && !again && prev && now - g.last_render_t < 0.005)
        g.stats.renderer_skips += (b->pushed + len - prev->pushed - 1) % len;
    g.last_render_t = now;

    Held *h = NULL;
    for (unsigned i = 0; i < g.held_n && !h; i++)
        if (held_at(i)->state == H_WAITING && held_at(i)->buf == buf && held_at(i)->index == index)
            h = held_at(i);
    if (h) {
        memcpy(h->data, buf, h->frames * 4);
        h->sound = index;
        h->state = H_READY;
        g.stats.rescued++;
        held_drain();
    } else {
        g.stats.skipped += b->render != 0;
        b->render = index;
    }
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

/* A device's pull, with the lock held: frames into g.scratch at the volume. Returns the frames that were queued;
 * *log_it says an underrun began that the log should name. */
static size_t pull_locked(size_t frames, int *log_it)
{
    if (frames > g.scratch_cap) {
        free(g.scratch);
        g.scratch = malloc(frames * 8);
        g.scratch_cap = g.scratch ? frames : 0;
    }
    if (!g.scratch)
        return 0;
    if (T.want && g.kind == KIND_NULL)   /* paced mode: the device plays the tap's FIFO (silence while it opens or
                                          * closes); the queue is the N64 clock's, the null output's, never pulled */
        return tap_pull_locked(frames, log_it);
    int32_t *out = g.scratch;
    double now = clock_now();
    if (g.pushed && !g.paused && now - g.last_push < PLAYING_S && now - g.first_push > LEVEL_AFTER_S) {
        uint32_t level = (uint32_t)g.count + device_frames(g.held_frames);
        if (!g.stats.level_count || level < g.stats.level_min)
            g.stats.level_min = level;
        if (level > g.stats.level_max)
            g.stats.level_max = level;
        int first_of_span = g.stats.level_count == g.reported_stats.level_count;
        if (first_of_span || level < g.span_min)
            g.span_min = level;
        if (first_of_span || level > g.span_max)
            g.span_max = level;
        g.stats.level_sum += level;
        g.stats.level_count++;
    }
    need(frames);
    size_t got = g.ring ? ring_read(out, frames) : 0;
    if (g.kind != KIND_NULL && frames > g.stats.period_frames && frames < g.dev_rate) {   /* a device's pull */
        g.stats.period_frames = (uint32_t)frames;
        retarget();
    }
    if (got && g.fade_in) {   /* after an underrun the sound comes back from silence */
        uint32_t n = got < g.fade_in ? (uint32_t)got : g.fade_in;
        for (uint32_t i = 0; i < n; i++) {
            int32_t k = (int32_t)(FADE - g.fade_in + i + 1);
            out[2 * i] = out[2 * i] * k / (int32_t)FADE;
            out[2 * i + 1] = out[2 * i + 1] * k / (int32_t)FADE;
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
        int32_t *z = out + 2 * got;
        if (ran_dry(n, now)) {
            if (g.kind != KIND_NULL) {   /* the queue was too short for this device and host */
                g.target += TARGET_STEP;
                retarget();
            }
            *log_it = g.reported < REPORT_MAX;
            g.reported += (uint32_t)*log_it;
            g.underrun_at = now - g.t0;
        }
        /* the last frame fades to silence: a step to zero would click */
        for (size_t i = 0; i < f; i++) {
            int32_t k = (int32_t)(FADE - 1 - i);
            z[2 * i] = g.last[0] * k / (int32_t)FADE;
            z[2 * i + 1] = g.last[1] * k / (int32_t)FADE;
        }
        memset(z + 2 * f, 0, (n - f) * 8);
        g.last[0] = g.last[1] = 0;
        g.fade_in = FADE;
    }
    g.stats.pulls++;
    if (g.gain != 65536)   /* full volume leaves the samples as they are */
        for (size_t i = 0; i < 2 * frames; i++)
            out[i] = (int32_t)((int64_t)out[i] * g.gain >> 16);
    return got;
}

/* After a pull, without the lock: the underrun's log line, then the hook. */
static void after_pull(int log_it, void (*hook)(void))
{
    if (log_it == 2) {   /* paced mode's tap */
        lock();
        unsigned long long n = (unsigned long long)T.stats.underruns;
        double at = g.underrun_at;
        unlock();
        fprintf(stderr, "audio: tap: the FIFO ran dry (underrun %llu, %.1f s in): silence until it has its level "
                "again%s\n", n, at, n == REPORT_MAX ? " (the exit line counts the rest)" : "");
    } else if (log_it) {
        PcAudioStats st;
        pc_audio_get_stats(&st);
        fprintf(stderr, "audio: underrun %llu, %.1f s in: the queue target is now %u frames%s\n",
                (unsigned long long)st.underruns, g.underrun_at, (unsigned)st.target_frames,
                st.underruns == REPORT_MAX ? " (the exit line counts the rest)" : "");
    }
    if (hook)
        hook();
}

/* 16 bits can't carry the converter's overshoot past full scale: those samples are clamped and counted. */
size_t pc_audio_pull(int16_t *out, size_t frames)
{
    int log_it = 0;
    lock();
    size_t got = pull_locked(frames, &log_it);
    const int32_t *s = g.scratch;
    for (size_t i = 0; i < 2 * frames; i++) {
        int32_t v = s ? s[i] : 0;
        if (v > 32767 || v < -32768) {
            g.stats.clipped++;
            v = v > 0 ? 32767 : -32768;
        }
        out[i] = (int16_t)v;
    }
    void (*hook)(void) = g.pull_hook;
    unlock();
    after_pull(log_it, hook);
    return got;
}

size_t pc_audio_pull_f32(float *out, size_t frames)
{
    int log_it = 0;
    lock();
    size_t got = pull_locked(frames, &log_it);
    const int32_t *s = g.scratch;
    for (size_t i = 0; i < 2 * frames; i++)
        out[i] = s ? (float)s[i] * (1.0f / 32768.0f) : 0.0f;
    void (*hook)(void) = g.pull_hook;
    unlock();
    after_pull(log_it, hook);
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
    retarget();   /* paced mode: the tap's FIFO (tap_retarget) */
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

/* A line of the counters every `seconds` (0: none) while the game plays, from pc_audio_periodic; also
 * RUSH2049_AUDIO_STATS=seconds. */
void pc_audio_set_report_every(unsigned seconds)
{
    lock();
    g.report_every = seconds;
    g.report_at = clock_now();
    g.reported_stats = g.stats;
    T.reported = T.stats;
    unlock();
}

/* The main loop, about once a second. */
void pc_audio_periodic(void)
{
    if (!g.inited)
        return;
    lock();
    double now = clock_now();
    if (g.report_every && now - g.report_at >= g.report_every && g.stats.pushed_frames) {
        char span[48];
        snprintf(span, sizeof span, ", the last %.0f s to %.0f s in", now - g.report_at, now - g.t0);
        report(span, &g.reported_stats, g.span_min, g.span_max);
        if (T.on) {
            tap_report(span, &T.reported);
            T.reported = T.stats;
        }
        g.reported_stats = g.stats;
        g.report_at = now;
    }
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
