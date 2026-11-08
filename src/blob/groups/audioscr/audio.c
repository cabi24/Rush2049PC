/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Audio-options screen text (GC audioscr.c). draft, levers, closing lever.
 * game_results_render (0x800FF298, kept) = GC animText@audioscr.c; game_results_update (0x800FEE04, internal,
 * only caller game_results_render) = GC animButtonText@audioscr.c; TB_FlashTextColors = GC TB_FlashTextColors with
 * GC's parameter type union __anon_21da {u8 c[4]; u32 rgba;} (same 7 words as the locked Color4 single).
 * Labels are wrong (not results screens): proposed audio_animText / audio_animButtonText.
 * Shaping devices (all GC-attested shapes, disclosed):
 *  - inlined non-static helpers au_gfx_lock/au_gfx_unlock/au_font_set (HAL lock + font), au_CenterText /
 *    au_RightText (GC TB_CenterText/TB_RightText), au_SetJust (GC TB_SetJust), au_SetFont (void wrapper of the
 *    font helper, GC HAL_SetFont: +8 B inline area), au_validOption (GC validOption@audioscr.c = `return 1`,
 *    inlined: it switches off LICM/strength reduction in the option loops as in retail);
 * - the toupper loop as GC's unconditional ternary store with an explicit `!= 0` loop test: with
 *    `*charPtr` as the test and an if/else store, uopt keeps cvt.J(*charPtr) as its own PRE web beside the
 *    loop-carried load (one extra `move v1,v0`); this form gives them one register as retail.
 * No unused locals.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct OSMesgQueue OSMesgQueue;
typedef struct Color4 { u8 r, g, b, a; } Color4;
typedef struct AuIdx { u8 pad0[32]; u16 songStrings; u8 pad34[44]; u16 optionNames; u16 musicTypes; } AuIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; AuIdx *indices; char **strings; } TextState;
typedef struct Options { u8 pad[12]; s8 sfxVolume; s8 musicVolume; s8 pad14; s8 musicType; } Options;
typedef struct ButtonInfo { u8 pad[8]; f32 angle; u8 pad12[36]; u8 pos[16]; } ButtonInfo;

extern OSMesgQueue text_lock_queue;
extern TextState gText;
extern s32 audio_buttonsInited;                           /* buttonsInited */
extern s32 gGameState;
extern s8 audio_gControllerRemoved;                            /* gControllerRemoved */
extern s32 playerInPause;
extern s32 audio_menuRect_cx;                           /* menuRect.x (right edge of the option column) */
extern s32 gAudioMenuSelect;
extern s32 audio_gSong;                           /* gSong */
extern char *gSongNames[];                       /* gSongNames */
extern Options gOptions;
typedef union TextColor { u8 c[4]; u32 rgba; } TextColor;   /* GC union __anon_21da */
extern TextColor flashColors[2];                  /* flashColors */
extern ButtonInfo audio_buttonInfo[];                  /* buttonInfo (N64 copy starts at the first button) */
extern u8 audio_title_button_pos[];                          /* buttonInfo + 0x2f0 (title position) */
extern u8 gCamera[];                          /* gCamera */
extern s32 gJust[2];                        /* TB_SetJust pair */
extern s32 osRecvMesg(OSMesgQueue *, void **, s32), osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32);
extern u32 HAL_GetTextWidth(char *, s32);
extern s16 TB_GetFontLineHeight(void);
extern void TB_SetTextColor(s32);
extern void TB_DrawText(s16, s16, char *);
extern s32 TB_Sprintf(char *, char *, ...);
extern void HAL_SetTextDepth(f32);
extern void HAL_SetTextTranslucency(f32);
extern u8 *TB_Strcpy(u8 *, void *);
extern void ZOID_ProjectWindow(s32, void *, void *, void *, s16 *);
extern TextColor gFlashColor1, gFlashColor2;
void TB_FlashTextColors(TextColor first, TextColor second)   /* TB_FlashTextColors (GC: two union __anon_21da) */
{
    gFlashColor1.rgba = first.rgba;
    gFlashColor2.rgba = second.rgba;
}

void au_gfx_lock(void) { osRecvMesg(&text_lock_queue, 0, 1); }
void au_gfx_unlock(void) { osSendMesg(&text_lock_queue, 0, 0); }
s32 au_font_set(s32 font)
{
    s32 old;
    au_gfx_lock();
    old = HAL_SetFontInternal(font);
    au_gfx_unlock();
    return old;
}
s16 au_CenterText(char *text, s16 centerX)
{
    return centerX - (HAL_GetTextWidth(text, -1) >> 1);
}
s16 au_RightText(char *text, s16 rightX)
{
    return rightX - HAL_GetTextWidth(text, -1);
}

void au_SetJust(s32 h, s32 v)
{
    gJust[1] = v;
    gJust[0] = h;
}

void au_SetFont(s32 font)
{
    au_font_set(font);
}

s8 au_validOption(s32 i)
{
    return 1;
}

void audio_animButtonText(void)
{
    s32 i;
    s32 x;
    s32 y;
    char str[40];
    s16 screenPos[2];
    char *charPtr;

    HAL_SetTextDepth(0.0f);
    au_SetJust(0, 1);
    au_SetFont(13);
    TB_SetTextColor(1);
    ZOID_ProjectWindow(0, audio_title_button_pos, gCamera, 0, screenPos);
    x = screenPos[0];
    y = screenPos[1];
    TB_DrawText(au_CenterText(gText.fixed[52], x), y, gText.fixed[52]);
    au_font_set(11);
    for (i = 0; i < 4; i++) {
        if (audio_buttonInfo[i].angle > 0.69813168f && audio_buttonInfo[i].angle < 2.443461f) continue;
        if (!au_validOption(i)) continue;
        if (audio_buttonInfo[i].angle > 0.34906584f && audio_buttonInfo[i].angle < 2.7925267f) HAL_SetTextTranslucency(128.0f);
        else HAL_SetTextTranslucency(-1.0f);
        if (audio_buttonInfo[i].angle < 1.5707964f) {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(1);
        } else {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(22);
        }
        ZOID_ProjectWindow(0, audio_buttonInfo[i].pos, gCamera, 0, screenPos);
        x = screenPos[0];
        y = screenPos[1];
        x -= 55;
        TB_DrawText(x, y,
                      gText.strings[gText.indices->optionNames + i]);
        switch (i) {
        case 0:
            TB_Sprintf(str, "%d", gOptions.musicVolume);
            break;
        case 1:
            TB_Sprintf(str, "%d", gOptions.sfxVolume);
            break;
        case 2:
            if (audio_gSong == 12) TB_Strcpy((u8 *)str, gText.fixed[34]);
            else if (audio_gSong == 13) TB_Strcpy((u8 *)str, gText.strings[gText.indices->songStrings]);
            else TB_Strcpy((u8 *)str, gSongNames[audio_gSong]);
            break;
        case 3:
            TB_Strcpy((u8 *)str, gText.strings[gOptions.musicType + gText.indices->musicTypes]);
            break;
        }
        for (charPtr = str; *charPtr != 0; charPtr++) {
            *charPtr = (*charPtr >= 'a' && *charPtr <= 'z') ? *charPtr ^ 0x20 : *charPtr;
        }
        ZOID_ProjectWindow(0, audio_buttonInfo[i + 4].pos, gCamera, 0, screenPos);
        x = screenPos[0];
        y = screenPos[1];
        x -= 35;
        TB_DrawText(x, y, str);
    }
    au_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
}


s32 audio_animText(s32 arg0)
{
    s32 i;
    s32 x;
    s32 y;
    char str[40];
    char *charPtr;

    if (audio_buttonsInited == 1) {
        audio_animButtonText();
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    au_SetFont(13);
    TB_SetTextColor(1);
    if ((gGameState & 0x7C0000) && audio_gControllerRemoved) {
        TB_Sprintf(str, "CONTROLLER %d %s", playerInPause + 1, gText.fixed[233]);
        TB_DrawText(au_CenterText(str, 160), 10, str);
    } else {
        TB_DrawText(au_CenterText(gText.fixed[52], 160), 10, gText.fixed[52]);
    }
    y = TB_GetFontLineHeight() * 2 + 10;
    au_font_set(10);
    x = audio_menuRect_cx;
    for (i = 0; i < 4; i++) {
        if (!au_validOption(i)) continue;
        TB_SetTextColor(i == gAudioMenuSelect ? 22 : 1);
        TB_DrawText(au_RightText(gText.strings[gText.indices->optionNames + i], x - 5), y,
                      gText.strings[gText.indices->optionNames + i]);
        switch (i) {
        case 0:
            TB_Sprintf(str, "%d", gOptions.musicVolume);
            break;
        case 1:
            TB_Sprintf(str, "%d", gOptions.sfxVolume);
            break;
        case 2:
            if (audio_gSong == 12) TB_Strcpy((u8 *)str, gText.fixed[34]);
            else if (audio_gSong == 13) TB_Strcpy((u8 *)str, gText.strings[gText.indices->songStrings]);
            else TB_Strcpy((u8 *)str, gSongNames[audio_gSong]);
            break;
        case 3:
            TB_Strcpy((u8 *)str, gText.strings[gOptions.musicType + gText.indices->musicTypes]);
            break;
        }
        for (charPtr = str; *charPtr != 0; charPtr++) {
            *charPtr = (*charPtr >= 'a' && *charPtr <= 'z') ? *charPtr ^ 0x20 : *charPtr;
        }
        TB_DrawText(x + 5, y, str);
        y += TB_GetFontLineHeight();
    }
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
