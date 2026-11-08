/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct { s32 h, v; } TBJust;
extern TBJust gJust;                          /* gJust */
s16 TB_CenterText(void *text, s16 centerX);   /* TB_CenterText */
s16 TB_RightText(void *text, s16 rightX);      /* TB_RightText */
s16 TB_GetFontHeight(void);                       /* TB_GetFontHeight */
void HAL_SetTextPos(s16 x, s16 y);                   /* HAL_SetTextPos */
void HAL_DrawText(void *text, s16 numChars);  /* HAL_DrawText */

void TB_DrawText(s16 x, s16 y, void *text)
{
    if (gJust.h == 1) x = TB_CenterText(text, x);
    else if (gJust.h == 2) x = TB_RightText(text, x);
    if (gJust.v == 1) y -= TB_GetFontHeight() / 2;
    else if (gJust.v == 4) y -= TB_GetFontHeight();
    HAL_SetTextPos(x, y); HAL_DrawText(text, -1);
}
