/* CheckVisible.c (visuals): show or hide a car visual in the car's own window (MAT3 visuals.c,
 * CheckVisible()). */
#include "game/game.h"


/* Show visual v's object in window `camera` if `on`, else hide it there, and return `on`; a car no window follows
 * (camera < 0) is shown or hidden in every window (CheckVisibleAll). The animators use it to keep the parts a
 * window's own view hides (the cockpit view's body and wings) out of that window only. */
s32 CheckVisible(tVisual *v,s32 on,s32 camera)
{
    if(camera<0) return CheckVisibleAll(v,on);
    if(on) MBOX_ShowObject(v->objnum,0,1<<camera);
    else MBOX_HideObject(v->objnum,1,1<<camera);
    return on;
}
