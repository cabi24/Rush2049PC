/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef float f32;
typedef unsigned int u32;
typedef unsigned short u16;
extern f32 gDepth;
extern u32 gFontAttribute;
extern void BlitZDist(u16);
void HAL_SetTextDepth(f32 value) {
    gDepth = value;
    if (value > 0.0f) {
        gFontAttribute |= 0x10;
        BlitZDist((u16)(u32)value);
    } else {
        gFontAttribute &= ~0x10;
    }
}
