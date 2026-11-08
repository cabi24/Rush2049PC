/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9).
 * field1C is the light slot (N64 +0x1C): the caller passes a ComponentLights, overlay A's own data. */
typedef struct {u32 flags,w4;void *obj;f32 scale,w;unsigned short model;s16 child,next;s16 field1A;uintptr_t field1C;void *tex[4],*rect,*clip,*pal;u32 prim,env;} Record;
extern Record ObjInstanceList[];
void ZOID_SetObjLights(s32 index,s32 halfword,uintptr_t word) {Record *r=&ObjInstanceList[index];r->field1A=halfword;r->field1C=word;}
