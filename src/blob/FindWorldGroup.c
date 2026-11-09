#include "game/game.h"

typedef struct Box104 {u8 pad0[52];f32 origin[3];u8 pad64[4];s16 next;u8 pad70[10];f32 minimum[3];f32 maximum[3];} Box104;

s32 FindWorldGroup(f32 *position) {
 f32 offset[3];Box104 *base=WorldBase,*box=base;s32 next;
 for(;;) {
  offset[0]=position[0]-box->origin[0];offset[1]=position[1]-box->origin[1];offset[2]=position[2]-box->origin[2];
  if(box->minimum[0]<=offset[0]&&offset[0]<=box->maximum[0]&&box->minimum[1]<=offset[1]&&box->minimum[2]<=offset[2]&&offset[2]<=box->maximum[2])return ZOID_FindObjInstance(box);
  next=box->next;if(next<0)break;box=&base[next];
 }
 return -1;
}
