/* FindWorldGroup.c: the world group a point is in (MAT3 world.c, FindWorldGroup()). */
#include "game/game.h"

/* A world part (MAT3 tModelPart), the fields used here. */
typedef struct tModelPart {
    u8 pad0[52];
    f32 pos[3];         /* +0x34 its place (the position row of its tUVMatrix mat at +0x10) */
    u8 pad64[4];
    s16 next;           /* +0x44 the next part at this level, an index into WorldBase; -1 none */
    u8 pad70[10];
    f32 min[3];         /* +0x50 its bounding box, relative to pos */
    f32 max[3];         /* +0x5C */
} tModelPart;

/* The scene node of the first top-level world part whose bounding box holds `position`, -1 if none. The box's top
 * (max[1]) is not tested, so a point above a part is in it. */
s32 FindWorldGroup(f32 *position) {
 f32 offset[3];tModelPart *base=WorldBase,*part=base;s32 next;
 for(;;) {
  offset[0]=position[0]-part->pos[0];offset[1]=position[1]-part->pos[1];offset[2]=position[2]-part->pos[2];
  if(part->min[0]<=offset[0]&&offset[0]<=part->max[0]&&part->min[1]<=offset[1]&&part->min[2]<=offset[2]&&offset[2]<=part->max[2])return ZOID_FindObjInstance(part);
  next=part->next;if(next<0)break;part=&base[next];
 }
 return -1;
}
