/* WeightedPriority: an fx tag's rank for the priority pass (MAT3 audiomgr.c's _WeightedPriority). */
#include "game/game.h"
#include "game/audiomgr.h"

/* The tag's priority, plus up to 80 the quieter it is, plus 8 for a loop; a one-shot with a priority that is
 * playing also gains a point per quarter second it has played. Lower ranks first: the pass walks the tags from
 * the lowest (OrderFx) and takes the voices of those past the quota (UpdateFx), so quiet sounds, loops and old
 * one-shots lose theirs first. */
s32 WeightedPriority(tFxSequence *tag) {
    s32 result;

    result = tag->priority;
    result = result + (1.0f - tag->vol) * 80.0f;
    if (tag->loop) {
        result += 8;
    }
    else if (tag->priority && tag->startTime >= 0.0f && tag->state == kFxPlaying) {
        result += AudioTimeoutPerc(tag->startTime, 0.25f);
    }
    return result;
}
