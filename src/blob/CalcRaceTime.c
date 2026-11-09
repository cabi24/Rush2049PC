/* CalcRaceTime: the time a race allows (MAT3 checkpoint.c). */
#include "game/game.h"

/* The ghost races' times, by track (ROM data), as an array. */

/* Returns the race's time limit in seconds: a ghost race on tracks 0 to 5 its track's (ghostRaceTimes), any other
 * race 10 minutes. */
f32 CalcRaceTime(void)
{
    f32 r;
    if ((gRace == kGhost) && (gTrackNum >= 0) && (gTrackNum < 6)) {
        r = ghostRaceTimes[gTrackNum];
    } else {
        r = 600.0f;
    }
    return r;
}
