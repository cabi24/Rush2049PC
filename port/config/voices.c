/* port/config/voices.c: [audio] voices and [audio] engines at run time (docs/port/AUDIO.md 10, FRONTEND.md).
 *
 * voices: MusyX's voice count, set by main at start (pc_voices_set) before the game threads run and never after: the
 * library is set up once a run (AudioInitSys), and its SFDs, voice pool and caches are sized from it then. 32
 * (original) gives the N64's set-up exactly; 48, 64 and 96 give the effects all but the songs' 16 voices, the sound
 * manager a budget of 3/4 of those, and 128 positional emitters (pc_snd_setup).
 *
 * engines: which drones' engines play. original is the game's own choice (the sound manager's priority pass keeps the
 * loudest within its budget); nearest N keeps, per listener (each player's view in split screen), the N nearest
 * drones' engines, with hysteresis (a drone that has its place keeps it until another is nearer by a fifth) and a
 * quarter-second fade in and out, and silences the others' (gain 0: the pass then holds them back and stops them as
 * it stops any silent loop); all plays every drone's engine and keeps them out of the budget, which the effects then
 * have to themselves. It can change at any time (the overlay): the emitter pass reads it each retrace.
 *
 * The counters (--log-voices) are filled by the game's sound code on its threads; relaxed atomics, read when every
 * N64 thread waits. */
#include "config.h"
#include "pc_game_types.h"

#define FADE_STEPS 15        /* retraces a fade takes: 0.25 s */
#define HYSTERESIS 0.8f      /* a drone with its place keeps it until another's distance is below this times its own */
#define LISTENERS 5          /* the sound manager's listeners (D_80146188: at most 5) */

static volatile int s_voices = PC_MX_N64_VOICES;
static volatile int s_engines = PC_ENGINES_ORIGINAL;

void pc_voices_set(int voices)
{
    s_voices = voices <= PC_MX_N64_VOICES ? PC_MX_N64_VOICES : voices > PC_MX_VOICE_SLOTS ? PC_MX_VOICE_SLOTS
                                                                                             : voices;
}

int pc_mx_voices(void)
{
    return s_voices;
}

void pc_snd_setup(unsigned char *voices, unsigned char *sfx, int *emitters)
{
    if (*voices != PC_MX_N64_VOICES || s_voices == PC_MX_N64_VOICES)
        return;   /* the set-up without the Expansion Pak, or original: the N64's */
    *voices = (unsigned char)s_voices;
    *sfx = (unsigned char)(s_voices - 16);
    *emitters = PC_SND_EMITTERS;
}

void pc_engines_set(int mode)
{
    s_engines = mode == PC_ENGINES_ALL || (mode >= 1 && mode < PC_MAX_LINKS) ? mode : PC_ENGINES_ORIGINAL;
}

int pc_engines(void)
{
    return s_engines;
}

/* ---------------------------------------------------------------- the counters */

static unsigned long long s_count[PC_SND_COUNTERS];
static int s_msg_most;

void pc_snd_count(int what)
{
    if (what >= 0 && what < PC_SND_COUNTERS)
        __atomic_fetch_add(&s_count[what], 1, __ATOMIC_RELAXED);
}

void pc_snd_msg_taken(int index)
{
    if (index + 1 > __atomic_load_n(&s_msg_most, __ATOMIC_RELAXED))
        __atomic_store_n(&s_msg_most, index + 1, __ATOMIC_RELAXED);
}

void pc_snd_counters(unsigned long long out[PC_SND_COUNTERS], int *msg_most)
{
    for (int i = 0; i < PC_SND_COUNTERS; i++)
        out[i] = __atomic_load_n(&s_count[i], __ATOMIC_RELAXED);
    *msg_most = __atomic_load_n(&s_msg_most, __ATOMIC_RELAXED);
}

/* ---------------------------------------------------------------- engines */

/* The emitter pass (one a retrace, on the sound manager's thread) notes, for each drone engine layer it walks, its
 * distance to each listener; then pc_engines_choose picks per listener and moves each car's fade one step. */
static float s_dist[LISTENERS][PC_MAX_LINKS];   /* this pass: the nearest layer of each car, -1 none */
static unsigned char s_chosen[LISTENERS][PC_MAX_LINKS];   /* the last pass's choice per listener */
static int s_fade[PC_MAX_LINKS];                /* 0 silent .. FADE_STEPS full */
static unsigned char s_seen[PC_MAX_LINKS];      /* the last pass: the car had an engine emitter */
static int s_noted;
/* Every pass, whatever the mode: the drones' engine layers that have an emitter, and those whose tag plays (state 2):
 * how many engines are heard (--log-voices). */
static int s_layers, s_playing, s_layers_last, s_playing_last, s_playing_most;
static unsigned s_playing_cars, s_playing_cars_last;

/* The car whose engine layer (0 or 1) this emitter's handle is: a positional engine is a drone's (a human's two are
 * not emitters, high_scores_display). -1 if none. */
static int engine_slot(int handle)
{
    if (handle == -1)
        return -1;
    for (int i = 0; i < PC_MAX_LINKS; i++)
        if (pc_car_sounds_on[i] && (pc_engine_sounds[i].rec[0].handle == handle ||
                                    pc_engine_sounds[i].rec[1].handle == handle))
            return i;
    return -1;
}

void pc_engine_state(int handle, int state)
{
    int i = engine_slot(handle);
    if (i < 0)
        return;
    s_layers++;
    if (state == 2) {
        s_playing++;
        s_playing_cars |= 1u << i;
    }
}

void pc_engines_pass_end(void)   /* the priority pass starts after the emitter pass: its counts are complete */
{
    s_layers_last = s_layers, s_playing_last = s_playing, s_playing_cars_last = s_playing_cars;
    if (s_playing > s_playing_most)
        s_playing_most = s_playing;
    s_layers = s_playing = 0;
    s_playing_cars = 0;
}

void pc_engines_heard(int *layers, int *playing, unsigned *cars, int *most)
{
    *layers = s_layers_last, *playing = s_playing_last, *cars = s_playing_cars_last, *most = s_playing_most;
}

int pc_engines_choosing(void)
{
    if (s_engines == PC_ENGINES_ORIGINAL)
        return 0;
    if (!s_noted) {
        for (int l = 0; l < LISTENERS; l++)
            for (int i = 0; i < PC_MAX_LINKS; i++)
                s_dist[l][i] = -1.0f;
        s_noted = 1;
    }
    return 1;
}

void pc_engines_note(int handle, int listener, float distance)
{
    int i = engine_slot(handle);
    if (i < 0 || listener < 0 || listener >= LISTENERS)
        return;
    if (s_dist[listener][i] < 0.0f || distance < s_dist[listener][i])
        s_dist[listener][i] = distance;
}

void pc_engines_choose(int listeners)
{
    int mode = s_engines;
    unsigned char want[PC_MAX_LINKS] = {0};
    if (listeners > LISTENERS)
        listeners = LISTENERS;
    for (int l = 0; l < listeners; l++) {
        unsigned char next[PC_MAX_LINKS] = {0};
        for (int k = 0; k < (mode == PC_ENGINES_ALL ? PC_MAX_LINKS : mode); k++) {
            int best = -1;
            float bestd = 0.0f;
            for (int i = 0; i < PC_MAX_LINKS; i++) {
                float d = s_dist[l][i];
                if (d < 0.0f || next[i])
                    continue;
                if (s_chosen[l][i])
                    d *= HYSTERESIS;   /* the place it has counts for a fifth of its distance */
                if (best < 0 || d < bestd)
                    best = i, bestd = d;
            }
            if (best < 0)
                break;
            next[best] = 1;
        }
        for (int i = 0; i < PC_MAX_LINKS; i++) {
            s_chosen[l][i] = next[i];
            want[i] |= next[i];
        }
    }
    for (int l = listeners; l < LISTENERS; l++)
        for (int i = 0; i < PC_MAX_LINKS; i++)
            s_chosen[l][i] = 0;
    for (int i = 0; i < PC_MAX_LINKS; i++) {
        int seen = 0;
        for (int l = 0; l < listeners; l++)
            seen |= s_dist[l][i] >= 0.0f;
        if (!seen) {
            s_seen[i] = 0;
            continue;
        }
        if (!s_seen[i]) {   /* an engine just made (a race's start, a resurrection): it starts as it plays, no fade */
            s_seen[i] = 1;
            s_fade[i] = want[i] ? FADE_STEPS : 0;
            continue;
        }
        if (want[i] && s_fade[i] < FADE_STEPS)
            s_fade[i]++;
        else if (!want[i] && s_fade[i] > 0)
            s_fade[i]--;
    }
    s_noted = 0;
}

float pc_engine_gain(int handle, float gain)
{
    int i;
    if (s_engines == PC_ENGINES_ORIGINAL || (i = engine_slot(handle)) < 0 || !s_seen[i])
        return gain;   /* not a drone's engine emitter this pass */
    return gain * (float)s_fade[i] / (float)FADE_STEPS;
}

/* The engines on at the last pass, as a bit per car (--log-voices). */
unsigned pc_engines_on(void)
{
    unsigned bits = 0;
    for (int i = 0; i < PC_MAX_LINKS; i++)
        if (s_engines != PC_ENGINES_ORIGINAL && s_seen[i] && s_fade[i] > 0)
            bits |= 1u << i;
    return bits;
}

/* ---------------------------------------------------------------- the priority pass's budget */

static int s_exempt;   /* this pass: drone engines walked so far that `all` keeps out of the budget */

void pc_snd_walk_begin(void)
{
    pc_engines_pass_end();
    s_exempt = 0;
    pc_snd_count(PC_SND_PASSES);
}

int pc_snd_over_budget(unsigned int tag, int rank, float budget)
{
    if (s_engines == PC_ENGINES_ALL && engine_slot((int)tag) >= 0) {
        s_exempt++;
        return 0;
    }
    if ((float)(rank - s_exempt) >= budget) {
        pc_snd_count(PC_SND_OVER_BUDGET);
        return 1;
    }
    return 0;
}
