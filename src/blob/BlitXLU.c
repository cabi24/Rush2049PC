/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct { u8 a; u8 b; u8 c; u8 d; } S;
extern S blitPrimColor;
extern intptr_t blitColors;
void BlitColors(void *arg0, s32 arg1, s32 arg2, s32 arg3);
void BlitSetAttribute(s32 arg0);

void BlitXLU(u16 arg0) {
    if (blitPrimColor.d != arg0) {
        blitPrimColor.d = arg0;
        blitColors = 0;
    }
    BlitColors(&blitPrimColor, 4, 1, 0);
    BlitSetAttribute(32);
}
