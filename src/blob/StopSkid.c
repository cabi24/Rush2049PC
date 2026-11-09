#include "game/game.h"

/* PC: the skid effect's host layout (skid_marks' EffectObj, InitSkids's PcEffectObj): its two links and its
 * display handle are pointers, so the midpoint is at +0x20 and the flag at +0x30. */
typedef struct {void *pad0[2];f32 f8;void *dl;f32 midpoint[3];u32 pad1C;u32 ready;} Dest;
typedef struct Pending {Dest *dest;f32 first[3],second[3];} Pending;
void StopSkid(Pending *p) {
 s32 i;
 Dest *d=p->dest;
 for(i=0;i<3;i++) {
  d->midpoint[i]=(p->first[i]+p->second[i])*0.5f;
 }
 d->ready=1;
 p->dest=NULL;
}
