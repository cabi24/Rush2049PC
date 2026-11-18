/* StopSkid.c: end a tire's current skid mark (MAT3 skids.c, _StopSkid()).
 *
 * UpdateSkids (groups/skid_marks) calls it when a tire stops skidding, or ContinueSkid when the mark must start
 * again (the tire turned too far from the mark's line); AnimateSkids then fades the mark. */
#include "game/game.h"

/* Stop the skid `ns` is laying: the mark keeps its middle (between where it started and where it has got to, for
 * the far-mark search when the pool runs out), is flagged active so AnimateSkids fades it, and the tire lets go
 * of it. */
void StopSkid(tNewSkid *ns) {
 s32 i;
 tSkid *s=ns->skid;
 for(i=0;i<3;i++) {
  s->pos[i]=(ns->start[i]+ns->pos[i])*0.5f;
 }
 s->active=1;
 ns->skid=NULL;
}
