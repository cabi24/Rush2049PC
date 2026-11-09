#include "game/game.h"


#define FIELD(p,t,o) (*(t *)((u8 *)(p)+(o)))

void InitDemoPathPoint(void) {
 s32 index,next;
 if(gBackwards)index=gTrackNum+6;else index=gTrackNum;
 next=cur_demo_ppoint[index]+1;
 if(next<0||next>=num_demo_ppoints[index])next=0;
 cur_demo_ppoint[index]=next;
 gDemoPathPt=demo_path_points[index][next];
}
