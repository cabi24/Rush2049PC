/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/* Image A formatted menu callback, [0x803AF744, 0x803AF978).
 * N64-specific renderer; witnessed sibling: A:803AE51C.
 * The callback argument is unused but homed by the original O32 body.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;
/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct MenuText {
    char *pad0[344 / 4];
    char *text344;
    char *pad348[4 / 4];
    char *text352;
    char *pad356[4 / 4];
    char *text360;
    char *pad364[244 / 4];
    char *text608;
    char *text612;
    char *text616;
    char *pad620[4 / 4];
    char *text624;
    char *pad628[316 / 4];
    char *text944;
} MenuText;
extern MenuText *gText_single;
extern u8 num_paks_found;
extern s8 gPakScr;
extern char warningscr_message_format[];
void HAL_SetTextDepth(f32);
void TB_SetJust(u32, u32);
void *HAL_SetFont(s32);
void TB_SetTextColor(s32);
s32 PS_GetBlocks(s32);
s32 sprintf(char *, const char *, ...);
void TB_DrawTextScrollBox(s16, s16, s16, s16, s16, s16, void *);
s16 TB_GetFontLineHeight(void);
void TB_DrawText(s16, s16, void *);

s32 warningscr_animText(s32 arg0)
{
    char buffer[256];
    s32 y;
    s32 height;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    HAL_SetFont(11);
    TB_SetTextColor(1);
    sprintf(buffer, warningscr_message_format,
            num_paks_found > 0 ? gText_single->text612 : gText_single->text608, gText_single->text616,
            gText_single->text352, gText_single->text360,
            PS_GetBlocks(0x810), gText_single->text344);
    TB_DrawTextScrollBox(160, 90, 300, 220, -1, 0, buffer);
    height = TB_GetFontLineHeight();
    y = 200 - height * 3;
    HAL_SetFont(gPakScr ? 11 : 10);
    TB_SetTextColor(gPakScr ? 1 : 22);
    TB_DrawText(160, y - (gPakScr ? 0 : 1), gText_single->text944);
    if (num_paks_found > 0) {
        HAL_SetFont(gPakScr ? 10 : 11);
        TB_SetTextColor(gPakScr ? 22 : 1);
        TB_DrawText(160, y - (gPakScr ? 1 : 0) + height, gText_single->text624);
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
