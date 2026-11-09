/* pause_TitlePane: the pane behind the pause screen's title (MAT3 pausescr.c's _TitlePane). */
#include "game/game.h"

#define PAUSE_TITLE_FONT 13

#define INPUT_SIZE 80   /* an input record (gPlayers): its pointer at +0x48 is 8 bytes (POINTERS.md 10) */
/* A player's record as this reads it: the handle of the player's profile first. */
typedef struct { void **save; u8 rest[INPUT_SIZE - 8]; } PausePlayer;
typedef struct { s16 x, y; } PauseTitlePos;
VIEW(PauseTitlePos, pause_titlePos);
#define pause_titlePos pause_titlePos_view
VIEW(PausePlayer[], gPlayers_save);
#define gPlayers_save gPlayers_save_view

/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);
void ChangePaneSize(void *,s32,s32,s32,s32);

/* Sizes the pane behind the pause screen's title to the title in font 13, centred on pause_titlePos, making the
 * pane the first time. The title: the controller's number and a message when a controller came out, the restart or
 * the quit question, else the pausing player's name and the pause title (strings of gText); on the rumble screen the
 * pane is put off screen. (MAT3 checks that the player has a profile before reading its name; the N64 doesn't.) */
void pause_TitlePane(void)
{
    s32 x,y,width;
    unsigned char *str;
    unsigned char text[64];
    HAL_SetFont(PAUSE_TITLE_FONT);
    str = text;
    if(pause_gControllerRemoved) {
        TB_Sprintf(text,pause_title_format,playerInPause+1,gText.single[233]);
        width=HAL_GetTextWidth(str,-1);
    } else if(gPauseState==kPauseRestart || gPauseState==kPauseQuit) {
        width=HAL_GetTextWidth(gText.taba[gText.tab[56]+gPauseState],-1);
    } else if(gPauseState==kPauseRumbleScreen) {
        width=0;
    } else {
        TB_Sprintf(text,pause_title_name_format,(u8 *)*gPlayers_save[playerInPause].save+PROFILE_OFS(20),gText.taba[gText.tab[56]]);
        width=HAL_GetTextWidth(str,-1);
    }
    x=pause_titlePos.x;
    y=pause_titlePos.y;
    if(!pause_gTitlePane) pause_gTitlePane=InitPane(-20,0,-10,10,176,0,0,0);
    if(!width) {
        ChangePaneSize(pause_gTitlePane,-20,0,-10,10);
    } else {
        ChangePaneSize(pause_gTitlePane,x-width/2-8,y-4,width/2+x+8,TB_GetFontLineHeight()+y+4);
    }
}
