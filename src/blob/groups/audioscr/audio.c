#include "game/game.h"

typedef struct AuIdx { u8 pad0[32]; u16 songStrings; u8 pad34[44]; u16 optionNames; u16 musicTypes; } AuIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; AuIdx *indices; char **strings; } TextState;
typedef struct Options { u8 pad[12]; s8 sfxVolume; s8 musicVolume; s8 pad14; s8 musicType; } Options;
typedef struct ButtonInfo { u8 pad[8]; f32 angle; u8 pad12[36]; u8 pos[16]; } ButtonInfo;

VIEW(TextState, gText);
#define gText gText_view

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(Options, gOptions);
#define gOptions gOptions_view

VIEW(TextColor[2], flashColors);
#define flashColors flashColors_view
VIEW(ButtonInfo[], audio_buttonInfo);
#define audio_buttonInfo audio_buttonInfo_view

VIEW(u8[], gCamera);
#define gCamera gCamera_view
VIEW(s32[2], gJust);
#define gJust gJust_view

void TB_FlashTextColors(TextColor first, TextColor second)   /* TB_FlashTextColors (GC: two union __anon_21da) */
{
    gFlashColor1.rgba = first.rgba;
    gFlashColor2.rgba = second.rgba;
}


void au_SetJust(s32 h, s32 v)
{
    gJust[1] = v;
    gJust[0] = h;
}

void au_SetFont(s32 font)
{
    HAL_SetFont(font);
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
    ZOID_ProjectWindow(0, audio_buttonInfo[11].pos, gCamera, 0, screenPos);   /* the title's position */
    x = screenPos[0];
    y = screenPos[1];
    TB_DrawText(TB_CenterText(gText.fixed[52], x), y, gText.fixed[52]);
    HAL_SetFont(11);
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
        TB_DrawText(TB_CenterText(str, 160), 10, str);
    } else {
        TB_DrawText(TB_CenterText(gText.fixed[52], 160), 10, gText.fixed[52]);
    }
    y = TB_GetFontLineHeight() * 2 + 10;
    HAL_SetFont(10);
    x = audio_menuRect.cx;
    for (i = 0; i < 4; i++) {
        if (!au_validOption(i)) continue;
        TB_SetTextColor(i == gAudioMenuSelect ? 22 : 1);
        TB_DrawText(TB_RightText(gText.strings[gText.indices->optionNames + i], x - 5), y,
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
