/* StopSkid.c: end a tire's current skid mark (MAT3 skids.c, _StopSkid()).
 *
 * UpdateSkids (groups/skid_marks) calls it when a tire stops skidding, or ContinueSkid when the mark must start
 * again (the tire turned too far from the mark's line); AnimateSkids then fades the mark. */
#include "game/game.h"

/* PC: the skid mark's host layout (skid_marks' EffectObj, InitSkids's PcEffectObj): its two links and its
 * polygon handle are pointers, so pos is at +0x20 and active at +0x30. */
typedef struct tSkid {void *ll[2];f32 lastTime;void *poly;f32 pos[3];u32 color;u32 active;} tSkid;
/* A tire's skid state (skid_marks' WheelSlot, MAT3 tNewSkid): the mark it is laying and the mark's two ends. */
typedef struct tNewSkid {tSkid *skid;f32 start[3],end[3];} tNewSkid;

/* Stop the skid `ns` is laying: the mark keeps its middle (between where it started and where it has got to, for
 * the far-mark search when the pool runs out), is flagged active so AnimateSkids fades it, and the tire lets go
 * of it. */
void StopSkid(tNewSkid *ns) {
 s32 i;
 tSkid *s=ns->skid;
 for(i=0;i<3;i++) {
  s->pos[i]=(ns->start[i]+ns->end[i])*0.5f;
 }
 s->active=1;
 ns->skid=NULL;
}
