/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef float f32;
#ifdef TARGET_PC
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9).
 * field1C is the light slot (N64 +0x1C): the caller passes a CompLight, overlay A's own data. */
typedef struct {u32 flags,w4;void *obj;f32 scale,w;unsigned short model;s16 child,next;s16 field1A;uintptr_t field1C;void *tex[4],*rect,*clip,*pal;u32 prim,env;} Record;
#else
typedef struct {u8 pad00[26];s16 field1A;u32 field1C;u8 pad20[36];} Record;
#endif
extern Record D_8012E700[];
void func_800A79B8(s32 index,s32 halfword,uintptr_t word) {Record *r=&D_8012E700[index];r->field1A=halfword;r->field1C=word;}
