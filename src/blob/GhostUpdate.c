/* GhostUpdate: a ghost's controls, played back or recorded, each model step (MAT3 ghost.c). */
#include "game/game.h"

/* A ghost's header (GhostSaveHeader, the N64 layout kept on the PC) and its controls: one byte a step of gear and
 * buttons, then one of steering, then one of gas and brake (4 bits each). */
typedef struct ReplayStream {
    u8 pad0[5];
    s8 mode;
    u8 pad6[0x34 - 6];
    f32 dt;
    u8 pad38[0x4C - 0x38];
    u32 data;           /* a heap block (the window): the ghost header keeps its N64 layout, file data */
    u32 pos;
    u32 count;
} ReplayStream;
/* PC: a ghost's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10): its header handle at +56 */
typedef struct ReplayOwner { void *ll; void **fileh; u8 pad10[0x24 - 0x08]; void **datah; ReplayStream **stream; } ReplayOwner;
#define DATA(s) ((u8 *)PC_N64P((s)->data))
typedef struct ReplaySlot { ReplayOwner *owner; } ReplaySlot;

#define ABS(x) ((x) >= 0 ? (x) : -(x))

/* One model step of car `idx`'s ghost: *out is the step's time (the ghost's own, played back). A ghost played back
 * (mode positive) sets the car's controls from its next step (none, once it ends); one recorded from a frame rate
 * other than this machine's (a step of 1/50 or 1/60 second) skips a step (-1) or takes an extra one (1) every few
 * steps to keep time. A recording (mode negative) stores the car's controls until the car finishes or the ghost is
 * full. Returns 0 to step normally. */
s32 GhostUpdate(s32 idx, f32 *out)
{
    ReplayStream *s;
    f32 f;

    *out = gRetraceTime;
    if (gEndingGhosts) {
        return 0;
    }
    if (gGhosts[idx] == 0) {
        return 0;
    }
    s = *gGhosts[idx]->owner->stream;
    if (s->mode == 0) {
        return 0;
    }
    if (s->mode > 0) {
        *out = s->dt;
        if (gRetraceTime != s->dt && 0.02f == s->dt) {
            if (ABS(gModelIteration) % 6 == 2) {
                return -1;
            }
        }
        if (s->pos == s->count) {
            PC_MODEL_ABORT(idx) = 0;
            PC_MODEL_WING(idx) = 0;
            PC_MODEL_STEER(idx) = 0.0f;
            PC_MODEL_GAS(idx) = 0.0f;
            PC_MODEL_BRAKE(idx) = 0.0f;
        } else {
            PC_MODEL_GEAR(idx) = (s8) (DATA(s)[s->pos] & 7) - 1;
            PC_MODEL_ABORT(idx) = (DATA(s)[s->pos] & 8) >> 3;
            PC_MODEL_WING(idx) = (DATA(s)[s->pos] & 0x10) >> 4;
            PC_MODEL_STEER(idx) = (f32) ((s8 *) DATA(s))[s->pos + s->count] / 127;
            PC_MODEL_GAS(idx) = (f32) (DATA(s)[s->pos + s->count + s->count] >> 4) / 15.0f;
            PC_MODEL_BRAKE(idx) = (f32) (DATA(s)[s->pos + s->count + s->count] & 0xF) / 15.0f;
            s->pos++;
        }
        if (gRetraceTime != s->dt && 0.016666668f == s->dt) {
            if (ABS(gModelIteration) % 5 == 2) {
                return 1;
            }
        }
        return 0;
    } else {
        if (PC_CAR_PLACE_LOCKED(idx)) {
            return 0;
        }
        if (!(s->pos < s->count)) {
            return 0;
        }
        DATA(s)[s->pos] = (PC_MODEL_GEAR(idx) + 1) | (PC_MODEL_ABORT(idx) * 8) | (PC_MODEL_WING(idx) * 16);
        if (PC_MODEL_STEER(idx) * 127.0f < 0.0f) {
            f = PC_MODEL_STEER(idx) * 127.0f - 0.5f;
        } else {
            f = PC_MODEL_STEER(idx) * 127.0f + 0.5f;
        }
        DATA(s)[s->pos + s->count] = (s32) f;
        DATA(s)[s->pos + s->count + s->count] = ((u8) (PC_MODEL_GAS(idx) * 15.0f + 0.5f) * 16) | (u8) (PC_MODEL_BRAKE(idx) * 15.0f + 0.5f);
        s->pos++;
        return 0;
    }
}
