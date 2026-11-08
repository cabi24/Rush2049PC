/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/* Image A menu text callback. N64-specific rendering; no arcade donor found.
 * Complete native extent: [0x803A6A28, 0x803A6B50), 296 bytes.
 * Typed callback and draw-state sequence follow the witnessed A:803AE51C
 * sibling. The unused argument is part of the externally addressed callback.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;
/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct MenuText {
    char *pad0[920 / 4];
    u8 *text920;
    char *pad924[12 / 4];
    u8 *text936;
} MenuText;
extern MenuText *gText_single;
extern u32 gContLevels;
extern s8 expansion_pak_present;
extern u8 licensescr_build_info_format[];
extern s32 build_date_text, build_time_text;
void HAL_SetTextDepth(f32);
void *HAL_SetFont(s32);
s8 HAL_SetFontFixedWidth(s8);
void HAL_SetFontSpacing(s16);
void TB_SetJust(u32, u32);
void TB_SetTextColor(s32);
void TB_DrawTextScrollBox(s16, s16, s16, s16, s16, s16, u8 *);
void TB_Printf(s16, s16, u8 *, ...);
void TB_DrawText(s16, s16, void *);

s32 licensescr_animText(s32 arg0)
{
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(0);
    HAL_SetFontFixedWidth(0);
    HAL_SetFontSpacing(2);
    TB_SetJust(1, 1);
    TB_SetTextColor(1);
    TB_DrawTextScrollBox(160, 165, 288, 110, -1, 0, gText_single->text936);
    if (gContLevels == 0x3C000) {
        TB_SetTextColor(14);
        TB_Printf(210, 30, licensescr_build_info_format, build_date_text, build_time_text);
    }
    if (expansion_pak_present != 0) {
        TB_SetTextColor(14);
        TB_DrawText(160, 10, gText_single->text920);
    }
    TB_SetJust(0, 3);
    HAL_SetFontFixedWidth(1);
    HAL_SetFontSpacing(-1);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
