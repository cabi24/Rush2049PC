/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned short u16;
typedef unsigned int u32;
extern u32 gBgColor;
void ZOID_SetBGColor(u16 color) {
    gBgColor = (color << 16) | color;
}
