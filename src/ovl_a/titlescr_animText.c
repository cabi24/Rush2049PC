/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef int s32; typedef unsigned int u32; typedef float f32;
/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices; value940 is entry 235, a string, which TB_DrawText (TB_DrawText) takes as a pointer */
typedef struct { char *pad0[128 / 4]; void *text128; char *pad132[808 / 4]; char *value940; } Countdown;
void HAL_SetTextDepth(f32 value);
void TB_SetJust(s32 a, s32 b);
void HAL_SetFont(s32 id);
void TB_SetTextColor(s32 id);
void TB_DrawTextScrollBox(s32 x, s32 y, s32 w, s32 h, s32 color, s32 flags, void *text);
void TB_DrawText(s16 x, s16 y, void *text);
extern s8 attract_alt_text_flag;
extern u8 titlescr_prompt_text[];
extern Countdown *gText_single;

s32 titlescr_animText(s32 arg0)
{
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    if (attract_alt_text_flag == 1) {
        HAL_SetFont(13);
        TB_SetTextColor(22);
        TB_DrawTextScrollBox(160, 190, 320, 110, -1, 0, titlescr_prompt_text);
    } else {
        HAL_SetFont(11);
        TB_SetTextColor(22);
        TB_DrawTextScrollBox(160, 185, 320, 110, -1, 0, gText_single->text128);
        TB_SetTextColor(0);
        HAL_SetFont(12);
        TB_DrawText(160, 205, gText_single->value940);
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
