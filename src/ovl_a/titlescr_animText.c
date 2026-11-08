/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef int s32; typedef unsigned int u32; typedef float f32;
/* The text tables (port/assets/text.c fills them with host pointers): single, the strings by index. */
typedef struct TextTables {
    unsigned short numSingle; unsigned char **single;
    unsigned short numTab, numTabA; unsigned short *tab; unsigned char **taba;
    unsigned short numTab2, numTab2a, numTab2b; unsigned short *tab2; unsigned short *tab2a; unsigned char **tab2b;
} TextTables;
void HAL_SetTextDepth(f32 value);
void TB_SetJust(s32 a, s32 b);
void HAL_SetFont(s32 id);
void TB_SetTextColor(s32 id);
void TB_DrawTextScrollBox(s32 x, s32 y, s32 w, s32 h, s32 color, s32 flags, void *text);
void TB_DrawText(s16 x, s16 y, void *text);
extern s8 attract_alt_text_flag;
extern u8 titlescr_prompt_text[];
extern TextTables gText;

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
        TB_DrawTextScrollBox(160, 185, 320, 110, -1, 0, gText.single[32]);
        TB_SetTextColor(0);
        HAL_SetFont(12);
        TB_DrawText(160, 205, gText.single[235]);
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
