/* The start menu's text and which rows it offers (MAT3 startscr.c animText, animButtonText and optionValid). */
#include "game/game.h"

typedef TextColor Color4;   /* the flashing colours (include/game's TextColor) */
typedef struct ButtonInfo { f32 x,y,angle; u8 unknown0C[36]; f32 position[3]; } ButtonInfo;
VIEW(ButtonInfo[], startscr_buttonInfo);
#define startscr_buttonInfo startscr_buttonInfo_view
VIEW(f32[3], startscr_title_button_position);
#define startscr_title_button_position startscr_title_button_position_view
VIEW(f32[3], startscr_players_button_position);
#define startscr_players_button_position startscr_players_button_position_view

VIEW(Color4, flashColors_at_1);
#define flashColors_at_1 flashColors_at_1_view

/* PC: game.pc_setup (port/config/start_menu.c, docs/port/FRONTEND.md "PC SETUP"): option slot 8 is PC SETUP while it
   is on, its label the port's; game.cheats_menu (port/config/cheats.c) makes slot 6, the cheat screen, valid. */
int pc_start_valid(int index, int value);
const char *pc_start_label(int index, const char *label);
#define PC_START_LABEL(i, s) ((void *)pc_start_label((i), (s)))

#define TEXT_START_TITLE 39         /* gText.single[]: the menu's title */
#define TEXT_START_ROWS 20          /* gText.tab[]: the rows' names */
#define START_ROWS 10
#define START_ROW_PLAYERS 0
#define START_ROW_CHEAT 6
#define START_ROW_DEBUG 7
#define DEBUG_BUTTON 0x100          /* game button 8: held, it offers the debug row */

/* The menu's text over its 3D buttons: the title on its button, each offered row's name beside its button (skipped
 * while the button faces away, half see-through while it turns, flashing on the selected one), and after the player
 * count's row the counts 1-4 (grey past the controllers plugged in, the chosen one flashing while the row is
 * selected). */
void startscr_animButtonText(void)
{
    s32 i;
    s32 j,x;
    f32 y;
    f32 *position;
    char text[32];
    s16 point[2];
    HAL_SetTextDepth(0.0f);
    TB_SetJust(kTB_JustLeft, kTB_JustCenter);
    ZOID_ProjectWindow(0,startscr_title_button_position,&gCamera[0],0,point);
    x=point[0]; y=point[1];
    HAL_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawText(TB_CenterText(gText.single[TEXT_START_TITLE],x),(s16)y,gText.single[TEXT_START_TITLE]);
    HAL_SetFont(10);
    for(i=0;i<START_ROWS;i++) {
        if(!startscr_optionValid(i))continue;
        position=startscr_buttonInfo[i].position;
        if(startscr_buttonInfo[i].angle>0.6981317f && startscr_buttonInfo[i].angle<2.443461f)continue;
        if(startscr_buttonInfo[i].angle>0.34906584f && startscr_buttonInfo[i].angle<2.7925267f)HAL_SetTextTranslucency(128.0f);
        else HAL_SetTextTranslucency(-1.0f);
        if(startscr_buttonInfo[i].angle<1.5707964f) {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            HAL_SetTextColor(224,224,224,255);
        } else {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(kTB_FlashText);
        }
        ZOID_ProjectWindow(0,position,&gCamera[0],0,point);
        x=point[0]-35; y=point[1];
        TB_DrawText(x,(s16)y,PC_START_LABEL(i,gText.taba[gText.tab[TEXT_START_ROWS]+i]));
        if(i==START_ROW_PLAYERS) {
            ZOID_ProjectWindow(0,startscr_players_button_position,&gCamera[0],0,point);
            x=point[0]-18; y=point[1];
            for(j=0;j<4;j++) {
                TB_Sprintf(text,startscr_button_player_count_format,j+1);
                if(gStartMenuSelect==START_ROW_PLAYERS) {
                    if(j>=gMaxPlayers)TB_SetTextColor(kTB_MedGryText);
                    else if(j+1==gNumPlayers)TB_SetTextColor(kTB_FlashText);
                    else TB_SetTextColor(0);
                } else {
                    if(j>=gMaxPlayers)TB_SetTextColor(kTB_MedGryText);
                    else TB_SetTextColor(0);
                }
                TB_DrawText(TB_CenterText(text,x),(s16)y,text);
                x+=16;
            }
        }
    }
    HAL_SetTextTranslucency(-1.0f);
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
}

/* The menu's text blit callback: over the 3D buttons once they are up, else a flat list (the title, the rows, the
 * player counts with the chosen one shadowed). Always 1. */
s32 startscr_animText(void *callback_context)
{
    s32 i;
    char text[48];
    s32 j,x;
    f32 y;
    if(startscr_buttonsInited==1) {
        startscr_animButtonText();
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    TB_SetJust(kTB_JustCenter, kTB_JustTop);
    HAL_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    y=10.0f;
    TB_DrawText(160,(s16)y,gText.single[TEXT_START_TITLE]);
    TB_GetFontLineHeight();
    HAL_SetFont(10);
    y=40.0f;
    for(i=0;i<START_ROWS;i++) {
        if(!startscr_optionValid(i))continue;
        if(i==gStartMenuSelect)TB_SetTextColor(kTB_FlashText);
        else HAL_SetTextColor(224,224,224,255);
        TB_DrawText(160,(s16)y,PC_START_LABEL(i,gText.taba[gText.tab[TEXT_START_ROWS]+i]));
        if(i==START_ROW_PLAYERS) {
            x=230;
            for(j=0;j<4;j++) {
                TB_Sprintf(text,startscr_player_count_format,j+1);
                if(gStartMenuSelect==START_ROW_PLAYERS) {
                    if(j+1==gNumPlayers)TB_SetTextColor(kTB_FlashText);
                    else HAL_SetTextColor(64,64,64,255);
                } else if(j+1==gNumPlayers) {
                    HAL_SetTextColor(0,0,0,255);
                    TB_DrawText(x+1,(s16)(y+1.0f),text);
                    HAL_SetTextColor(224,224,224,255);
                } else {
                    HAL_SetTextColor(64,64,64,255);
                }
                TB_DrawText(x,(s16)y,text);
                x+=16;
            }
        }
        y+=22.5f;
    }
    HAL_SetTextTranslucency(-1.0f);
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

/* Is the menu's row `index` offered? By its startscr_validOption entry; the debug row only while game button 8 is
 * held, the cheat menu once the start menu's code has been entered. The PC settings can add PC SETUP and the cheat
 * menu (PC_START_VALID). */
s8 startscr_optionValid(s32 index)
{
    s8 *entry = &startscr_validOption[index];
    s32 value = *entry;

    if (index == START_ROW_DEBUG && !(gActiveLevels & DEBUG_BUTTON)) {
        value = 0;
    }
    if (index == START_ROW_CHEAT) {
        value = gCheatActive[CHEAT_MENU_UNLOCKED];
    }
    return PC_START_VALID(index, value);
}
