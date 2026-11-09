#include "game/game.h"

/* PC: the record is an fx tag (audiomgr_thread.c's tFxSequence: state, loop, priority, startTime, vol), whose
 * links and list are host-sized (docs/port/SYNTH.md). */
typedef struct Record {
    void *unknown00[3];
    unsigned int unknown18;
    s32 kind;
    u8 unknown20[5];
    s8 flag;
    u8 unknown26;
    u8 value;
    u8 unknown28[4];
    f32 start;
    f32 scale;
} Record;

s32 WeightedPriority(Record *record) {
    s32 result;

    result = record->value;
    result = result + (1.0f - record->scale) * 80.0f;
    if (record->flag) {
        result += 8;
    }
    else if (record->value && record->start >= 0.0f && record->kind == 2) {
        result += AudioTimeoutPerc(record->start, 0.25f);
    }
    return result;
}
