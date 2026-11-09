#include "game/game.h"

typedef struct Color4 { u32 word; } Color4;
typedef struct Language { s32 unknown0; char **text; s32 unknown8; u16 *indices; char **indexed; } Language;
typedef struct Slot { f32 x,y,angle; u8 unknown0C[36]; f32 position[3]; u8 alpha,unknown3D[3]; } Slot;
typedef struct Camera { f32 uv[3][3],position[3]; u8 unknown30[104]; } Camera;

VIEW(f32[3], trackscr_title_button_position);
#define trackscr_title_button_position trackscr_title_button_position_view

VIEW(Slot[], trackscr_buttonInfo);
#define trackscr_buttonInfo trackscr_buttonInfo_view
VIEW(Camera[], gCamera);
#define gCamera gCamera_view
VIEW(Language, gText);
#define gText gText_view
VIEW(Color4, flashColors);
#define flashColors flashColors_view
VIEW(Color4, flashColors_at_1);
#define flashColors_at_1 flashColors_at_1_view

VIEW(u8[][5], gCircuitTracks);
#define gCircuitTracks gCircuitTracks_view

extern int sprintf(char *,const char *,...);

extern void TB_FlashTextColors(Color4,Color4);

s32 trackscr_animText(void *callback_context)
{
    if (trackscr_buttonsInited == 1) trackscr_animButtonText();
    return 1;
}

/* animButtonText@trackscr.c (MAT3): inlined into animText trackscr_animText; deleted stub 0x803AECDC */
void trackscr_animButtonText(void)
{
    s32 i;
    char text[80];
    char *label;
    s16 point[2];
    f32 position[3];
    s32 index;
    s32 x;
    s32 y;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(0, 1);
    ZOID_ProjectWindow(0, trackscr_title_button_position, &gCamera[0], 0, point);
    x = point[0];
    y = point[1];
    HAL_SetFont(13);
    TB_SetTextColor(1);
    TB_Sprintf(text, gText.text[45]);
    TB_DrawText(TB_CenterText(text, x), y, text);
    if (!expansion_pak_present && gTrackNum == 5) {
        position[2] = trackscr_buttonInfo[0].position[2];
        position[0] = 0.0f;
        position[1] = trackscr_buttonInfo[0].position[1];
        position[1] -= trackscr_gButtonSpace * trackscr_gButtonScale;
        HAL_SetFont(11);
        ZOID_ProjectWindow(0, position, &gCamera[0], 0, point);
        x = point[0];
        y = point[1];
        label = gText.text[232];
        TB_SetTextColor(22);
        TB_DrawText(TB_CenterText(label, x), y, label);
    }
    HAL_SetFont(11);
    for (i = 0; i < trackscr_gNumOptions; i++) {
        index = trackscr_gOpts[i];
        if ((trackscr_buttonInfo[index].angle <= 0.6981316804885864f || trackscr_buttonInfo[index].angle >= 2.4434609413146973f) && trackscr_buttonInfo[index].alpha) {
            HAL_SetTextTranslucency(trackscr_buttonInfo[index].alpha);
            ZOID_ProjectWindow(0, trackscr_buttonInfo[index].position, &gCamera[0], 0, point);
            x = point[0] - 35.0f * trackscr_gButtonScale;
            y = point[1];
            if (trackscr_buttonInfo[index].angle < 1.5707963705062866f) {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(22);
            }
            label = (gText.indexed + trackscr_gOpts[i])[gText.indices[1]];
            TB_DrawText(x, y, label);
        }
        if ((trackscr_buttonInfo[index + 21].angle <= 0.6981316804885864f || trackscr_buttonInfo[index + 21].angle >= 2.4434609413146973f) && trackscr_buttonInfo[index + 21].alpha) {
            HAL_SetTextTranslucency(trackscr_buttonInfo[index + 21].alpha);
            ZOID_ProjectWindow(0, trackscr_buttonInfo[index + 21].position, &gCamera[0], 0, point);
            x = point[0] - 35.0f * trackscr_gButtonScale;
            y = point[1];
            if (trackscr_buttonInfo[index + 21].angle < 1.5707963705062866f) {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(22);
            }
            switch (index) {
            case 0:
                label = gTrackTitles[gTrackNum];
                TB_DrawText(x, y, label);
                break;
            case 1:
                label = gText.indexed[gText.indices[35] + gGhostSelect];
                TB_DrawText(x, y, label);
                break;
            case 2:
                sprintf(text, trackscr_laps_format, gNumLaps);
                TB_DrawText(x, y, text);
                break;
            case 3:
                if (gOptions.a == 0) label = gText.text[208];
                else if (gOptions.a == 1) label = gText.text[130];
                else label = gText.text[12];
                TB_DrawText(x, y, label);
                break;
            case 4:
                TB_DrawText(x, y, (gText.indexed + (gOptions.b == 0 ? 0 : 1))[gText.indices[36]]);
                break;
            case 5:
                sprintf(text, trackscr_option5_format, gOptions.c);
                TB_DrawText(x, y, text);
                break;
            case 6:
                sprintf(text, trackscr_option6_format, gOptions.d);
                TB_DrawText(x, y, text);
                break;
            case 7:
                sprintf(text, trackscr_option7_format, gOptions.e);
                TB_DrawText(x, y, text);
                break;
            case 8:
                TB_DrawText(x, y, gOptions.f == 1 ? gText.text[208] : gText.text[130]);
                break;
            case 9:
                sprintf(text, trackscr_option9_format, gOptions.g);
                TB_DrawText(x, y, text);
                break;
            case 10:
                sprintf(text, trackscr_option10_format, gOptions.h);
                TB_DrawText(x, y, text);
                break;
            case 11:
                TB_DrawText(x, y, (gText.indexed + (gOptions.i == 0 ? 0 : 1))[gText.indices[16]]);
                break;
            case 12:
                TB_DrawText(x, y, (gText.indexed + (gBackwards == 0 ? 0 : 1))[gText.indices[16]]);
                break;
            case 13:
                if (gRace == 3) {
                    TB_DrawText(x, y, (gText.indexed + (gCircuitTracks[gCircuitTrack][2] == 0 ? 0 : 1))[gText.indices[16]]);
                } else if (gOptions.mode == 2) {
                    TB_DrawText(x, y, gText.indexed[gText.indices[18] + 2]);
                } else {
                    TB_DrawText(x, y, (gText.indexed + (gOptions.mode == 0 ? 0 : 1))[gText.indices[16]]);
                }
                break;
            case 17:
                sprintf(text, trackscr_drones_format, gNumDrones);
                TB_DrawText(x, y, text);
                break;
            case 20:
                TB_DrawText(x, y, (gText.indexed + (gDeathMode == 0 ? 0 : 1))[gText.indices[16]]);
                break;
            }
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(-1.0f);
}


