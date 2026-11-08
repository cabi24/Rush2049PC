/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/* Update the four-byte cached state only when it changes, then invalidate it. */
extern unsigned char gFontColor[4];
extern intptr_t blitColors;
void HAL_SetTextColor(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
    if (a != gFontColor[0] || b != gFontColor[1] || c != gFontColor[2] || d != gFontColor[3]) {
        gFontColor[0] = a;
        gFontColor[1] = b;
        gFontColor[2] = c;
        gFontColor[3] = d;
        blitColors = 0;
    }
}
