#include "game/game.h"

typedef struct Record {u32 flags,w4;void *obj;f32 scale,w;unsigned short model;s16 child,next;s16 field1A;uintptr_t field1C;void *tex[4],*rect,*clip,*pal;u32 prim,env;} Record;
VIEW(Record[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
void ZOID_SetObjLights(s32 index,s32 halfword,uintptr_t word) {Record *r=&ObjInstanceList[index];r->field1A=halfword;r->field1C=word;}
