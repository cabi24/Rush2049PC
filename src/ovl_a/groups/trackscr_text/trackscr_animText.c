/* The track select screen's text (MAT3 trackscr.c animText and animButtonText): the title, and each race option's
 * name and value beside its 3D buttons. */
#include "game/game.h"

typedef TextColor Color4;   /* the flashing colours (include/game's TextColor) */

VIEW(f32[3], trackscr_title_button_position);
#define trackscr_title_button_position trackscr_title_button_position_view

VIEW(Color4, flashColors);
#define flashColors flashColors_view
VIEW(Color4, flashColors_at_1);
#define flashColors_at_1 flashColors_at_1_view

VIEW(u8[][5], gCircuitTracks);
#define gCircuitTracks gCircuitTracks_view

extern int sprintf(char *,const char *,...);


/* gText's entries */
#define TEXT_TRACK_TITLE 45             /* single[]: the screen's title */
#define TEXT_NEEDS_EXPANSION 232        /* single[]: the track needs an Expansion Pak */
#define TEXT_OFF_ON 16                  /* tab[]: off, on */
#define TEXT_MIRROR_SECRET 18           /* tab[]: + 2, the mirror option's hidden value */
#define TEXT_GHOST_CHOICES 35           /* tab[]: the ghosts option's values */
#define TEXT_OPTION4_VALUES 36          /* tab[]: option 4's two values */
#define TEXT_OPTION_NAMES 1             /* tab[]: the options' names */

/* The screen's text blit callback: the buttons' text, once they exist. Always 1. */
s32 trackscr_animText(void *callback_context)
{
    if (trackscr_buttonsInited == 1) trackscr_animButtonText();
    return 1;
}

/* The title over its button; on TRACK_NEEDS_EXPANSION without an Expansion Pak a flashing notice under it; then for
 * each offered option, while its buttons face the camera, its name at the label button and its value at the
 * value button (the selected ones flashing): the track's title, the ghosts, laps, the stunt and battle settings,
 * direction, mirror (a circuit's race's own, or the secret third value), drones, death mode. */
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
    TB_SetJust(kTB_JustLeft, kTB_JustCenter);
    ZOID_ProjectWindow(0, trackscr_title_button_position, &gCamera[0], 0, point);
    x = point[0];
    y = point[1];
    HAL_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    TB_Sprintf(text, gText.single[TEXT_TRACK_TITLE]);
    TB_DrawText(TB_CenterText(text, x), y, text);
    if (!expansion_pak_present && gTrackNum == TRACK_NEEDS_EXPANSION) {
        position[2] = trackscr_buttonInfo[0].position[2];
        position[0] = 0.0f;
        position[1] = trackscr_buttonInfo[0].position[1];
        position[1] -= trackscr_gButtonSpace * trackscr_gButtonScale;
        HAL_SetFont(11);
        ZOID_ProjectWindow(0, position, &gCamera[0], 0, point);
        x = point[0];
        y = point[1];
        label = gText.single[TEXT_NEEDS_EXPANSION];
        TB_SetTextColor(kTB_FlashText);
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
                TB_SetTextColor(kTB_WhiteText);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(kTB_FlashText);
            }
            label = gText.taba[gText.tab[TEXT_OPTION_NAMES] + trackscr_gOpts[i]];
            TB_DrawText(x, y, label);
        }
        if ((trackscr_buttonInfo[index + TRACK_OPTS].angle <= 0.6981316804885864f || trackscr_buttonInfo[index + TRACK_OPTS].angle >= 2.4434609413146973f) && trackscr_buttonInfo[index + TRACK_OPTS].alpha) {
            HAL_SetTextTranslucency(trackscr_buttonInfo[index + TRACK_OPTS].alpha);
            ZOID_ProjectWindow(0, trackscr_buttonInfo[index + TRACK_OPTS].position, &gCamera[0], 0, point);
            x = point[0] - 35.0f * trackscr_gButtonScale;
            y = point[1];
            if (trackscr_buttonInfo[index + TRACK_OPTS].angle < 1.5707963705062866f) {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(kTB_WhiteText);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(kTB_FlashText);
            }
            switch (index) {
            case TRACK_OPT_TRACK:
                label = gTrackTitles[gTrackNum];
                TB_DrawText(x, y, label);
                break;
            case TRACK_OPT_GHOSTS:
                label = gText.taba[gText.tab[TEXT_GHOST_CHOICES] + gGhostSelect];
                TB_DrawText(x, y, label);
                break;
            case TRACK_OPT_LAPS:
                sprintf(text, trackscr_laps_format, gNumLaps);
                TB_DrawText(x, y, text);
                break;
            case TRACK_OPT_STUNT_MODE:
                if (gOptions.a == 0) label = gText.single[208];
                else if (gOptions.a == 1) label = gText.single[130];
                else label = gText.single[12];
                TB_DrawText(x, y, label);
                break;
            case 4:
                TB_DrawText(x, y, gText.taba[gText.tab[TEXT_OPTION4_VALUES] + (gOptions.b == 0 ? 0 : 1)]);
                break;
            case TRACK_OPT_STUNT_POINTS:
                sprintf(text, trackscr_option5_format, gOptions.c);
                TB_DrawText(x, y, text);
                break;
            case TRACK_OPT_STUNT_TIME:
                sprintf(text, trackscr_option6_format, gOptions.d);
                TB_DrawText(x, y, text);
                break;
            case TRACK_OPT_STUNT_DEATH:
                sprintf(text, trackscr_option7_format, gOptions.e);
                TB_DrawText(x, y, text);
                break;
            case TRACK_OPT_BATTLE_MODE:
                TB_DrawText(x, y, gOptions.f == 1 ? gText.single[208] : gText.single[130]);
                break;
            case TRACK_OPT_BATTLE_POINTS:
                sprintf(text, trackscr_option9_format, gOptions.g);
                TB_DrawText(x, y, text);
                break;
            case TRACK_OPT_BATTLE_TIME:
                sprintf(text, trackscr_option10_format, gOptions.h);
                TB_DrawText(x, y, text);
                break;
            case TRACK_OPT_BATTLE_11:
                TB_DrawText(x, y, gText.taba[gText.tab[TEXT_OFF_ON] + (gOptions.i == 0 ? 0 : 1)]);
                break;
            case TRACK_OPT_BACKWARDS:
                TB_DrawText(x, y, gText.taba[gText.tab[TEXT_OFF_ON] + (gBackwards == 0 ? 0 : 1)]);
                break;
            case TRACK_OPT_MIRROR:
                if (gRace == kCircuit) {
                    TB_DrawText(x, y, gText.taba[gText.tab[TEXT_OFF_ON] + (gCircuitTracks[gCircuitTrack][2] == 0 ? 0 : 1)]);
                } else if (gOptions.mode == MIRROR_SECRET) {
                    TB_DrawText(x, y, gText.taba[gText.tab[TEXT_MIRROR_SECRET] + 2]);
                } else {
                    TB_DrawText(x, y, gText.taba[gText.tab[TEXT_OFF_ON] + (gOptions.mode == 0 ? 0 : 1)]);
                }
                break;
            case TRACK_OPT_DRONES:
                sprintf(text, trackscr_drones_format, gNumDrones);
                TB_DrawText(x, y, text);
                break;
            case TRACK_OPT_DEATH_MODE:
                TB_DrawText(x, y, gText.taba[gText.tab[TEXT_OFF_ON] + (gDeathMode == 0 ? 0 : 1)]);
                break;
            }
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(-1.0f);
}
