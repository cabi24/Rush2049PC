/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
#define NULL ((void *)0)
typedef struct {u16 id,slot;u8 pad4[28];} Record;
typedef struct {u16 mode;u8 pad2[22];} Dest;
extern u16 stree_savenum;
extern Record *stree_save;
extern Dest *stree_surf;
void RemoveCollisionObject(u32 id) {s32 i;Record *p=stree_save;for(i=0;i<stree_savenum;i++,p++){if(p->id==id)stree_surf[p->slot].mode=15;}}
