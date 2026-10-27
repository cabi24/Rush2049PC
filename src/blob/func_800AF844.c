/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
#define NULL ((void *)0)
#ifdef TARGET_PC
/* PC: the skid effect's host layout (skid_marks' EffectObj, func_800B0580's PcEffectObj): its two links and its
 * display handle are pointers, so the midpoint is at +0x20 and the flag at +0x30. */
typedef struct {void *pad0[2];f32 f8;void *dl;f32 midpoint[3];u32 pad1C;u32 ready;} Dest;
#else
typedef struct {u8 pad0[16];f32 midpoint[3];u32 pad1C;u32 ready;} Dest;
#endif
typedef struct {Dest *dest;f32 first[3],second[3];} Pending;
void func_800AF844(Pending *p) {
 s32 i;
 Dest *d=p->dest;
 for(i=0;i<3;i++) {
  d->midpoint[i]=(p->first[i]+p->second[i])*0.5f;
 }
 d->ready=1;
 p->dest=NULL;
}
