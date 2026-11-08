/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image A projected seven-option callback: 964 fresh exact bytes.
 * Real 80399394 eligibility context and existing group identity are preserved.
 * Reconstructed text36 replaces unproven text50. Independently authenticated
 * formats and all six native language tables bound the only writer at17 bytes
 * including NUL for the entire signed16 count domain. Original capacity unknown.
 * The full stack census and stock diagnostic metadata establish frame144,
 * point88/text92, and nonoverlapping scalar homes. See.
 * No pad arrays, fake signatures, unused locals, artificial reads, register
 * forcing, inline assembly or modified compiler. Color4 is native by-value
 * word-aligned; no original typedef or arcade ancestor is claimed.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Color4 { u32 word; } Color4;
typedef struct Language { s32 unknown0; char **text; s32 unknown8; u16 *indices; char **indexed; } Language;
typedef struct Slot { f32 x,y,angle; u8 unknown0C[36]; f32 position[3]; u8 unknown3C[4]; } Slot;
typedef struct Camera { f32 uv[3][3], position[3]; u8 unknown30[104]; } Camera;
extern Language gText;
extern s32 modescr_buttonsInited, gModeMenuSelect;
extern f32 modescr_title_button_position[3];
extern Camera gCamera[];
extern s16 gNumPlayers;
extern Slot modescr_buttonInfo[];
extern char modescr_one_player_format[], modescr_players_format[];
extern Color4 flashColors, flashColors_at_1;
extern void HAL_SetTextDepth(f32);
extern s32 HAL_SetFont(s32);
extern void TB_SetTextColor(s32);
extern void TB_SetJust(u32,u32);
extern void ZOID_ProjectWindow(s32,f32 *,Camera *,void *,s16 *);
extern s16 TB_CenterText(void *,s16);
extern s16 TB_GetFontLineHeight(void);
extern void TB_DrawText(s16,s16,void *);
extern void TB_Sprintf(char *,char *,...);
extern s32 modescr_optionValid(s32);
extern void HAL_SetTextTranslucency(f32);
extern void TB_FlashTextColors(Color4,Color4);

s32 modescr_animText(void *callback_context)
{
    s16 x, y;
    s32 i;
    char *title;
    Slot *slot;
    char text[36];
    s16 point[2];
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    TB_SetTextColor(1);
    x = 160;
    y = 10;
    if (modescr_buttonsInited == 1) {
        TB_SetJust(0, 1);
        ZOID_ProjectWindow(0, modescr_title_button_position, &gCamera[0], 0, point);
        x = point[0];
        y = point[1];
    }
    title = gText.text[40];
    TB_DrawText(TB_CenterText(title, x), y, title);
    y += TB_GetFontLineHeight() * 2;
    if (gNumPlayers == 1) {
        TB_Sprintf(text, modescr_one_player_format, gNumPlayers, gText.text[29]);
    } else {
        TB_Sprintf(text, modescr_players_format, gNumPlayers, gText.indexed[gText.indices[20]]);
    }
    TB_SetTextColor(10);
    y += TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    for (i = 0; i < 7; i++) {
        if (modescr_optionValid(i)) {
            TB_SetTextColor(i == gModeMenuSelect ? 22 : 1);
            if (modescr_buttonsInited == 1) {
                slot = &modescr_buttonInfo[i];
                if (slot->angle > 0.6981317f && slot->angle < 2.443461f) continue;
                if (slot->angle > 0.34906584f && slot->angle < 2.7925267219543457f) {
                    HAL_SetTextTranslucency(128.0f);
                } else {
                    HAL_SetTextTranslucency(-1.0f);
                }
                if (slot->angle < 1.5707964f) {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(1);
                } else {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(22);
                }
                ZOID_ProjectWindow(0, slot->position, &gCamera[0], 0, point);
                x = point[0];
                y = point[1];
                x -= 45;
            }
            title = gText.indexed[gText.indices[22] + i];
            TB_DrawText(x, y, title);
            y += TB_GetFontLineHeight();
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors, flashColors_at_1);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
