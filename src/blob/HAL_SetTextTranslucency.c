/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned short u16;
extern float gTrans;
extern u32 gFontAttribute;
void BlitXLU(u16 a);
void HAL_SetTextTranslucency(float x) {
    gTrans = x;
    if (x < 0.0f || x >= 255.0f) {
        gFontAttribute &= ~0x20;
    } else {
        gFontAttribute |= 0x20;
        BlitXLU((u32)x);
    }
}
