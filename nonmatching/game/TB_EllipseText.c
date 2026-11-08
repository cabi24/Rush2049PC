/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* TB_EllipseText = TB_EllipseText (MAT3 TB_text.c, GC 0x8004F668, size 0x190). */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
extern s8 gSpacing; /* gSpacing (HAL_text.c static) */
u32 HAL_GetTextWidth(u8 *text, s16 numChars); /* HAL_GetTextWidth */
u8 *TB_Strcat(u8 *str1, u8 *str2);             /* TB_Strcat */
u8 *TB_Strcpy(u8 *str1, u8 *str2);             /* TB_Strcpy */
u32 TB_Strlen(u8 *s);                          /* TB_Strlen */
s8 HAL_GetFontSpacing(void);
void TB_EllipseText(void *dst, void *src, s16 width)
{
    u8 *d = dst; s32 w; s32 dbl; s32 len; u8 eStr[16]; s32 ew; const u8 *ellipse = (const u8 *)"...";
    w = HAL_GetTextWidth(src, -1);
    if (w <= width) TB_Strcpy(d, src);
    else {
        dbl = *d == 255;
        TB_Strcpy(d, src);
        len = TB_Strlen(d);
        if (len <= 1) return;
        len -= 2;
        if (dbl) {
            eStr[0] = 255; TB_Strcat(eStr, (u8 *)ellipse);
            eStr[7] = d[len*2+1]; eStr[8] = d[len*2+2];
            eStr[9] = d[len*2+3]; eStr[10] = d[len*2+4];
            eStr[11] = 0; eStr[12] = 0; d[len*2+1] = 0; d[len*2+2] = 0;
        } else {
            u8 *end;
            TB_Strcpy(eStr, (u8 *)ellipse);
            end = d + len;
            TB_Strcat(eStr, end);
            *end = 0;
        }
        ew = width - HAL_GetTextWidth(eStr, -1);
        while (w > ew - HAL_GetFontSpacing()) {
            len--;
            if (dbl) { d[len*2+1] = 0; d[len*2+2] = 0; } else d[len] = 0;
            if (len == 0) break;
            w = HAL_GetTextWidth(d, -1);
        }
        TB_Strcat(d, eStr);
    }
}
