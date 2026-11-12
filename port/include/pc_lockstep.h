/* port/include/pc_lockstep.h: what online play's lockstep stands on (docs/port/LOCKSTEP.md, NETPLAY.md 5 phase 0):
 * the pads gate at each retrace and the pads sources behind it (live input, a replay file, the network), the input
 * delay line, the record and replay files, and the state hash. PC-layer code only.
 *
 * Retrace numbers. n counts the retraces the main loop sends, from 0 for the first: the number --log-physics prints
 * as vi=. Retrace n's pads are latched before n's VI event, and n's hash is taken at the same moment, while every N64
 * thread waits (port/ultra/loop.c's snapshot): it covers the game as retraces 0 to n-1 left it, so two PCs that agree
 * on the pads of 0 to n-1 agree on hash n.
 *
 * The gate. With a source set (or --record-inputs, --input-delay, RUSH_SIM_GATE), the main loop asks for retrace n's
 * pads when n is due and before it does anything else there: it polls this PC's pads once (the window's events, live
 * input and scripts, as without a source), hands them to the source, and asks the source for the four pads. Until the
 * source has them, the loop waits: the N64 clock stays where it is, nothing is sent to the game, the window's events
 * are pumped (quitting works), and it asks again when pc_pads_signal is called or after PC_PADS_POLL_MS. Nothing of the
 * wait reaches the game: on the virtual clock (--fast) a run with waits is the run without, retrace for retrace
 * (ctest gate_same). Without a source the main loop is as it always was. */
#ifndef PC_LOCKSTEP_H
#define PC_LOCKSTEP_H

#include <stddef.h>
#include <stdint.h>

#include "pc_port.h"   /* PcPad */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- the state hash (port/ultra/statehash.c) */

#define PC_HASH_REGIONS 16
typedef struct {
    const char *name;        /* "models", "cars", ...; "heap", "own", "decoupled", "arenas" for the full hash's */
    uint64_t hash;           /* xxHash64 of its bytes, with host pointers normalised */
    uint32_t bytes;          /* its size */
    uint32_t pointers;       /* host pointers normalised in it */
    uint32_t unnamed;        /* of them, into the image where no table names the target: hashed as their offset in
                              * the image, which is the same in every process of one build but not across OSes */
} PcHashRegion;

typedef struct {
    uint32_t retrace;
    uint64_t state;          /* the curated regions' hash (pc_hash_enable): every retrace */
    int full;                /* 1: the game heap and the game's other data were hashed at this retrace too */
    uint64_t full_hash;      /* ... their hash; 0 when not */
    int n;                   /* regions, the curated ones first */
    PcHashRegion region[PC_HASH_REGIONS];
} PcStateHash;

/* Hashing on, from the next retrace: the curated regions every retrace, the game heap's block, own storage, the
 * decoupled data and the arenas every full_every retraces (0: never). Main thread. --log-hash N[,W] turns it on and
 * prints every Nth retrace's. */
void pc_hash_enable(uint32_t full_every);
int pc_hash_enabled(void);
void pc_hash_log(uint32_t every, uint32_t full_every);   /* --log-hash N[,W]: hashing on, a line every N retraces */
/* Retrace n is due (port/ultra/loop.c, after the gate and its snapshot): RUSH_SIM_DESYNC, then the hash when on, the
 * log line, and the source's hash callback. */
void pc_hash_retrace(uint32_t n);
const PcStateHash *pc_hash_last(void);   /* the last retrace's hash, NULL before the first */
/* Keep the curated regions' bytes, as hashed, for the last `retraces` retraces (0, the default: none), so a desync
 * found a round trip late still has the retrace's own state to report. */
void pc_hash_keep(uint32_t retraces);
/* The desync report (docs/port/LOCKSTEP.md 5): retrace n's hashes, region by region, then each curated region's bytes
 * as hashed (hex, host pointers normalised): n's when kept, else the last retrace's (the file says which). With the
 * full hash taken at n and still the last, the heap's, the arenas', own storage's and the decoupled data's normalised
 * bytes go to PATH.heap, PATH.arenas, PATH.own and PATH.decoupled, and the report lists where each arena, own-storage
 * symbol and decoupled item is in them. For the session to call on a mismatch; --hash-report N writes it at retrace N.
 * 0 on success. */
int pc_hash_report(const char *path, uint32_t n);
/* --hash-report N:PATH (offline: replay to the retrace two PCs first differ at, on each). */
void pc_hash_report_at(uint32_t n, const char *path);
/* xxHash64 (written from its specification, docs/port/LOCKSTEP.md 5): one call, and a stream. */
uint64_t pc_xxh64(const void *data, size_t size, uint64_t seed);
typedef struct {
    uint64_t total, v[4];
    uint8_t mem[32];
    uint32_t used;
} PcXxh64;
void pc_xxh64_init(PcXxh64 *s, uint64_t seed);
void pc_xxh64_update(PcXxh64 *s, const void *data, size_t size);
uint64_t pc_xxh64_digest(const PcXxh64 *s);
/* A host pointer into the game heap's block or the executable's image, as the hash takes it: what it points to, the
 * same in every process and on every OS where it can be (the heap and the offset in it; an own-storage symbol's N64
 * address and N64 offset, a game function's N64 address, a decoupled item and offset; elsewhere in the image only a
 * tag); 0 for a value outside both. Which values in the game's data are taken for pointers depends on the bytes
 * (port/ultra/statehash.c, docs/port/LOCKSTEP.md 5.2). */
uint64_t pc_hash_pointer(uint64_t v);
void pc_hash_image(uintptr_t *start, uintptr_t *end);   /* the executable's image as the hash sees it */

/* ---------------------------------------------------------------- the pads gate (port/ultra/lockstep.c) */

enum { PC_PADS_READY = 0, PC_PADS_WAIT = 1 };
#define PC_PADS_POLL_MS 5    /* a waiting gate asks again at least this often, pumping the window's events */

/* A pads source: where the game's pads come from instead of this PC's alone. The network layer is one; a replay file
 * is another (port/ultra/replay.c). Every callback is called on the main thread. */
typedef struct PcPadsSource {
    const char *name;        /* for the log */
    void *user;
    /* Retrace n, once, before pads(n) is first asked: this PC's own pads, as live input and the scripts give them
     * after all shaping (dead zone, sensitivity, the overlay's idle pads) and after --input-delay's line when set. A
     * network source sends its player's port for the retrace its delay assigns them to. May be NULL. */
    void (*local)(void *user, uint32_t n, const PcPad local[4]);
    /* Retrace n's four pads for the game: PC_PADS_READY with out[] filled (buttons, sticks, connected and rumble per
     * port), or PC_PADS_WAIT while they aren't known yet. */
    int (*pads)(void *user, uint32_t n, PcPad out[4]);
    /* Retrace n's state hash, when hashing is on (pc_hash_enable), after pads(n) and before n's VI event. May be NULL. */
    void (*hash)(void *user, uint32_t n, const PcStateHash *h);
} PcPadsSource;

/* The source from the next retrace on (NULL: none, this PC's pads alone). Main thread, before pc_ultra_run. 0, or -1
 * when one is set already (--replay-inputs and a session can't both own the pads). */
int pc_pads_set_source(const PcPadsSource *source);
/* Any thread: what a waiting source waited for may have come (a packet); the gate asks again at once. */
void pc_pads_signal(void);
/* The gate's counts, for a session's overlay and log. */
typedef struct {
    uint32_t retraces;       /* retraces through the gate */
    uint32_t waited;         /* of them, ones whose pads weren't there at once */
    uint64_t wait_ns;        /* real time waited in all */
    uint64_t longest_ns;     /* the longest wait */
} PcPadsStats;
void pc_pads_stats(PcPadsStats *out);

/* port/ultra/loop.c's side. pc_pads_gated: 1 when the gate is in the path. pc_pads_gate(n): retrace n is due; 0 when
 * its pads are ready (pc_pads_take then gives them), 1 when the loop is to come back (it waited up to
 * PC_PADS_POLL_MS, with the window's events pumped). pc_pads_take: n's pads, 1; 0 when the gate isn't in the path. */
int pc_pads_gated(void);
int pc_pads_gate(uint32_t n);
int pc_pads_take(uint32_t n, PcPad pads[4]);

/* ---------------------------------------------------------------- the input delay line (lockstep.c) */

/* Each side's own input delay (NETPLAY.md 11.2): the pads sampled at retrace n go to slot n + d. Growing d skips slots,
 * which get the last pads assigned again; shrinking it makes the sample's slot one already assigned, and the sample is
 * dropped. Slots before the first sample are idle (no buttons, the stick centred, the first sample's connected and
 * rumble flags). Delays 0 to PC_DELAY_MAX. */
#define PC_DELAY_MAX 240
#define PC_DELAY_RING 512
typedef struct {
    uint32_t from;           /* from this retrace on */
    int delay;
} PcDelayStep;
#define PC_DELAY_STEPS 32
/* "D" or "D:FROM,D:FROM,..." (--input-delay 2:0,4:600,1:1200: 2 from retrace 0, 4 from 600, 1 from 1200), sorted by
 * FROM, the first from 0. The steps' count, or -1 with why. */
int pc_delay_parse(const char *spec, PcDelayStep *steps, int max, char *why, size_t why_size);
int pc_delay_at(const PcDelayStep *steps, int n, uint32_t retrace);   /* the delay in force at a retrace */
typedef struct {
    PcPad slot[PC_DELAY_RING][4];
    int64_t last;            /* the last slot assigned; -1 none */
    PcPad last_pads[4];
    uint32_t dropped;        /* samples dropped (the delay shrank) */
    uint32_t repeated;       /* slots given the last pads again (it grew) */
} PcDelayLine;
void pc_delay_reset(PcDelayLine *d);
/* Retrace n's sample, at delay d: 1 when it took a slot, 0 when dropped. */
int pc_delay_put(PcDelayLine *d, uint32_t n, int delay, const PcPad sample[4]);
/* Slot n's pads: 0, or -1 when not assigned yet (or long gone). */
int pc_delay_get(const PcDelayLine *d, uint32_t n, PcPad out[4]);

/* ---------------------------------------------------------------- record, replay, options (lockstep.c, replay.c) */

typedef struct {
    const char *record;      /* --record-inputs FILE: each retrace's final pads */
    const char *replay;      /* --replay-inputs FILE: those pads instead of this PC's */
    int replay_force;        /* --replay-force: replay a file whose header doesn't match this run */
    const char *delay;       /* --input-delay D or D:FROM,...; NULL none */
    int log_pads;            /* --log-pads: each change of the game's pads, with its retrace */
    /* What a replay must match (docs/port/LOCKSTEP.md 3): "key=value" lines, made by main.c from the build, the
     * cartridge and the settings of NETPLAY.md 1.5; lines starting with '#' are said but not compared. */
    const char *must_match;
} PcLockstepOptions;
/* After the settings and the SI are set up, before pc_ultra_run: opens the files, parses the delay, reads
 * RUSH_SIM_GATE. 0, or 2 (the program's exit code) with the reason logged. */
int pc_lockstep_setup(const PcLockstepOptions *o);
void pc_lockstep_finish(void);   /* the run's end: the record file flushed and closed, the counts logged */

/* The record file (docs/port/LOCKSTEP.md 3), for a session that keeps its input log too. */
#define PC_PADFILE_MAGIC "R2PCPADS"
#define PC_PADFILE_VERSION 1u
#define PC_PADFILE_RECORD 20u
typedef struct PcPadWriter PcPadWriter;
PcPadWriter *pc_padfile_create(const char *path, const char *must_match, char *why, size_t why_size);
int pc_padfile_write(PcPadWriter *w, uint32_t n, const PcPad pads[4]);   /* retraces in order from 0 */
void pc_padfile_close(PcPadWriter *w);
typedef struct {
    char *must_match;        /* the header's text */
    uint8_t *records;        /* count records of PC_PADFILE_RECORD bytes, retrace 0 first */
    uint32_t count;
    uint32_t tail;           /* bytes of a record cut short at the end (a truncated file), 0 */
    uint32_t bad;            /* the first record whose check failed (count if none) */
} PcPadFile;
int pc_padfile_read(const char *path, PcPadFile *f, char *why, size_t why_size);   /* 0, or -1 with why */
void pc_padfile_record(const PcPadFile *f, uint32_t i, PcPad pads[4]);
void pc_padfile_free(PcPadFile *f);
/* The must-match lines of a and b that differ, one "key: a against b" a line, into out; the number that differ. */
int pc_padfile_mismatch(const char *a, const char *b, char *out, size_t size);

#ifdef __cplusplus
}
#endif

#endif
