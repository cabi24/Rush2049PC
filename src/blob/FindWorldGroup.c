/* FindWorldGroup.c: the world group a point is in (MAT3 world.c, FindWorldGroup()). */
#include "game/game.h"

/* The scene node of the first top-level world part whose bounding box holds `position`, -1 if none. The box's top
 * (max[1]) is not tested, so a point above a part is in it. */
s32 FindWorldGroup(f32 *position) {
 f32 offset[3];tModelPart *base=WorldBase,*part=base;s32 next;
 for(;;) {
  offset[0]=position[0]-part->mat.uvs[3][0];offset[1]=position[1]-part->mat.uvs[3][1];offset[2]=position[2]-part->mat.uvs[3][2];
  if(part->min[0]<=offset[0]&&offset[0]<=part->max[0]&&part->min[1]<=offset[1]&&part->min[2]<=offset[2]&&offset[2]<=part->max[2])return ZOID_FindObjInstance(part);
  next=part->next;if(next<0)break;part=&base[next];
 }
 return -1;
}
