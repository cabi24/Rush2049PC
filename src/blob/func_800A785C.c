/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed short s16;typedef unsigned char u8;typedef unsigned int u32;
#ifdef TARGET_PC
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9).
 * D_8012E738 is node 0's palette slot (N64 +0x38), a pointer: the PC version addresses the node. */
typedef struct Node {u32 flags,w4;void *obj;float scale,w;unsigned short model;s16 child,next,mode;void *light,*tex[4],*rect,*clip;uintptr_t pal;u32 prim,env;} Node;
extern Node D_8012E700[];
void func_800A785C(s16 index,uintptr_t value) {
 D_8012E700[index].pal=value;
}
#else
typedef struct Slot {u32 value;u8 pad4[64];} Slot;
extern Slot D_8012E738[];
void func_800A785C(s16 index,u32 value) {
 D_8012E738[index].value=value;
}
#endif
