/* InitDemoPathPoint.c: where the attract mode's race starts (MAT3 resurrect.c, InitDemoPathPoint()).
 *
 * Each demo race starts at the next of a few points along the drone path of its track (its own list for the
 * track run backwards), so that the attract mode doesn't show the same stretch every time. */
#include "game/game.h"

/* Pick the next demo start point of the current track (tracks 0-5, or 6-11 backwards): cur_demo_ppoint[track] goes
 * round num_demo_ppoints[track], and gDemoPathPt becomes that entry of demo_path_points[track], a path point
 * index (SetResurrectionData starts the cars there). */
void InitDemoPathPoint(void) {
 s32 track, next;
 if (gBackwards) track = gTrackNum + 6; else track = gTrackNum;
 next = cur_demo_ppoint[track] + 1;
 if (next < 0 || next >= num_demo_ppoints[track]) next = 0;
 cur_demo_ppoint[track] = next;
 gDemoPathPt = demo_path_points[track][next];
}
