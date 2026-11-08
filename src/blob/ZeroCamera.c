/* flags: -g0 -O2 -mips2 -G 0 -non_shared */

typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16; typedef signed int s32; typedef unsigned int u32; typedef float f32;
typedef struct PartSlot {
  u8 pad0[0x24]; f32 f24; f32 f28; f32 f2C; u8 pad30[0x60]; f32 f90; s8 f94; u8 pad95[3];
} PartSlot;
extern s32 unitmatrix;
extern PartSlot gCamera[];
void CopyUV(void *arg0, void *arg1);

void ZeroCamera(s16 arg0)
{
s32 i = ((arg0 & 1) ? 0 : (arg0 & 2) ? 1 : (arg0 & 4) ? 2 : 3);
 gCamera[i].f94 = i;
 gCamera[i].f24 = 0.0f; gCamera[i].f28 = 0.0f; gCamera[i].f2C = 0.0f;
 CopyUV(&unitmatrix, &gCamera[i]);
 gCamera[i].f90 = 0.0f;
}
