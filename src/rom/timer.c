/* The frame timer and the countdown clock (MAT3 timer.c): both count the scheduler's retraces
 * (scheduler_retrace_count, 60 or 50 a second). UpdateFrameTimer, once a frame, measures the frame (gLastFrameCount
 * retraces, gFrameSeconds) and the game time (gSeconds), which stops while paused; the countdown is a retrace count
 * to reach. */

#include "rom_tu.h"

extern volatile int scheduler_retrace_count;   /* gScheduler.retraceCount */
extern volatile int gLastFrameCounter;         /* the retrace count at the last frame */
extern f32 gRetraceTime;                       /* a retrace's seconds */
extern f32 gFrameSeconds;
extern f32 gSeconds;
extern u32 osSetIntMask(u32);
extern int gTimerPaused;
extern int gLastFrameCount;                    /* the last frame's retraces, at most 5 */
extern int gCountdownStart;                    /* the retrace count the countdown started at */
extern int gCountdownEnd;                      /* the retrace count it ends at */
extern f32 gRetraceRate;                       /* retraces a second */

/* Starts the frame timer: no frame counted yet, a frame and the game time both two retraces long. With interrupts
 * off. */
void InitializeTimer(void) {
    u32 mask;
    register f32 t;

    mask = osSetIntMask(1);
    gLastFrameCounter = scheduler_retrace_count;
    gFrameSeconds = t = 2.0f * gRetraceTime;
    gSeconds = t;
    osSetIntMask(mask);
}

/* Stops the game time (the countdown is moved on with it). */
void PauseTimer(void) {
    gTimerPaused = 1;
}

/* Starts the game time again. */
void ResumeTimer(void) {
    gTimerPaused = 0;
}

/* Once a frame: the retraces since the last frame (gLastFrameCount, then capped at 5, and gFrameSeconds their
 * time); while paused the countdown's start and end move on by them, else gSeconds is the time since the countdown
 * started.
 * PC: pc_game_frame_note tells the frame pacing (port/ultra) how long the frame was. */
void UpdateFrameTimer(void) {
    gLastFrameCount = scheduler_retrace_count - gLastFrameCounter;
    gLastFrameCounter = scheduler_retrace_count;
    if (gTimerPaused) {
        gCountdownStart += gLastFrameCount;
        gCountdownEnd += gLastFrameCount;
    } else {
        gSeconds = (scheduler_retrace_count - gCountdownStart) * gRetraceTime;
    }
    if (gLastFrameCount > 5) {
        gLastFrameCount = 5;
    }
    gFrameSeconds = gLastFrameCount * gRetraceTime; pc_game_frame_note(gLastFrameCount);
}

/* Starts the countdown: seconds from now (in retraces). */
void SetCountdownTimer(f32 seconds) {
    gCountdownStart = scheduler_retrace_count;
    gCountdownEnd = (int)(seconds * gRetraceRate) + scheduler_retrace_count;
}

/* Adds seconds to the countdown. */
void AddCountdownTime(f32 seconds) {
    gCountdownEnd += (int)(seconds * gRetraceRate);
}

/* The countdown's seconds left (negative once it has run out). */
f32 GetCountdownTimeRemaining(void) {
    return (gCountdownEnd - scheduler_retrace_count) * gRetraceTime;
}

/* Whether the countdown has run out. */
s32 CountdownTimeOut(void) {
    return (gCountdownEnd - scheduler_retrace_count) < 1;
}
