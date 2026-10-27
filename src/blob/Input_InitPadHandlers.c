/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef float f32;
/* The sprite list D_80140BF0: palette and texture are pointer-sized (particle_knot's Sprite on the PC). */
typedef struct {uintptr_t word0,word4;u16 half8,halfA,halfC,padE,half10,half12;u8 tail[12];} Record;
extern Record D_80140BF0[];
void Input_InitPadHandlers(u32 index,u32 h8,uintptr_t w4,uintptr_t w0,u32 hA,u32 hC,u32 h10,u32 h12) {Record *r=&D_80140BF0[index];r->word4=w4;r->word0=w0;r->half8=h8;r->halfA=hA;r->halfC=hC;r->half10=h10;r->half12=h12;}
