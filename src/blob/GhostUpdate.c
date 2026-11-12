/* GhostUpdate: a ghost's controls, played back or recorded, each model step (MAT3 ghost.c). */
#include "game/game.h"

/* gGhosts[i], car i's ghost: the handle of its record (tGhostFile). The record's header (tGhostHeader) is the replay's
 * state: flag positive plays back, negative records; frameTime the step it was recorded at; offset the step and size
 * the steps. The samples are three channels of size bytes, one byte a step each: gear and buttons, steering, then gas
 * and brake (4 bits each). */
typedef struct ReplaySlot { tGhostFile *ghost; } ReplaySlot;

#define ABS(x) ((x) >= 0 ? (x) : -(x))

/* The samples a header steps through (MAT3: header->start): a ghost played back reads its decoded recording, the
 * ghost's data (datah, DecodeGhost); a race being recorded writes its player's block (ghost_recording, NewGhost.c). */
static u8 *ghost_samples(tGhostFile *ghost, tGhostHeader *s)
{
    return s->flag > 0 ? *ghost->datah : *ghost_recording(s);
}

/* One model step of car `idx`'s ghost: *out is the step's time (the ghost's own, played back). A ghost played back
 * (flag positive) sets the car's controls from its next step (none, once it ends); one recorded from a frame rate
 * other than this machine's (a step of 1/50 or 1/60 second) skips a step (-1) or takes an extra one (1) every few
 * steps to keep time. A recording (flag negative) stores the car's controls until the car finishes or the ghost is
 * full. Returns 0 to step normally. */
s32 GhostUpdate(s32 idx, f32 *out)
{
    tGhostFile *ghost;
    tGhostHeader *s;
    u8 *data;
    f32 f;

    *out = gRetraceTime;
    if (gEndingGhosts) {
        return 0;
    }
    if (gGhosts[idx] == 0) {
        return 0;
    }
    ghost = gGhosts[idx]->ghost;
    s = *ghost->header;
    if (s->flag == 0) {
        return 0;
    }
    if (s->flag > 0) {
        *out = s->frameTime;
        if (gRetraceTime != s->frameTime && 0.02f == s->frameTime) {
            if (ABS(gModelIteration) % 6 == 2) {
                return -1;
            }
        }
        if (s->offset == s->size) {
            PC_MODEL_ABORT(idx) = 0;
            PC_MODEL_WING(idx) = 0;
            PC_MODEL_STEER(idx) = 0.0f;
            PC_MODEL_GAS(idx) = 0.0f;
            PC_MODEL_BRAKE(idx) = 0.0f;
        } else {
            data = ghost_samples(ghost, s);
            PC_MODEL_GEAR(idx) = (s8) (data[s->offset] & 7) - 1;
            PC_MODEL_ABORT(idx) = (data[s->offset] & 8) >> 3;
            PC_MODEL_WING(idx) = (data[s->offset] & 0x10) >> 4;
            PC_MODEL_STEER(idx) = (f32) ((s8 *) data)[s->offset + s->size] / 127;
            PC_MODEL_GAS(idx) = (f32) (data[s->offset + s->size + s->size] >> 4) / 15.0f;
            PC_MODEL_BRAKE(idx) = (f32) (data[s->offset + s->size + s->size] & 0xF) / 15.0f;
            s->offset++;
        }
        if (gRetraceTime != s->frameTime && 0.016666668f == s->frameTime) {
            if (ABS(gModelIteration) % 5 == 2) {
                return 1;
            }
        }
        return 0;
    } else {
        if (PC_CAR_PLACE_LOCKED(idx)) {
            return 0;
        }
        if (!(s->offset < s->size)) {
            return 0;
        }
        data = ghost_samples(ghost, s);
        data[s->offset] = (PC_MODEL_GEAR(idx) + 1) | (PC_MODEL_ABORT(idx) * 8) | (PC_MODEL_WING(idx) * 16);
        if (PC_MODEL_STEER(idx) * 127.0f < 0.0f) {
            f = PC_MODEL_STEER(idx) * 127.0f - 0.5f;
        } else {
            f = PC_MODEL_STEER(idx) * 127.0f + 0.5f;
        }
        data[s->offset + s->size] = (s32) f;
        data[s->offset + s->size + s->size] = ((u8) (PC_MODEL_GAS(idx) * 15.0f + 0.5f) * 16) | (u8) (PC_MODEL_BRAKE(idx) * 15.0f + 0.5f);
        s->offset++;
        return 0;
    }
}
