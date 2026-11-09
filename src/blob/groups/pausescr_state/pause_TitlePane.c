#include "game/game.h"


#define F(p,t,o) (*(t *)((u8 *)(p)+(o)))
/* PC: gText has host pointers (port/assets/text.c), so its header fields and string-table
 * entries sit at host offsets: TXH maps an N64 header offset, TXS an N64 string-table byte offset. */
#define TXH(o) ((o) == 4 ? 8 : (o) == 12 ? 24 : (o) == 16 ? 32 : (o))
#define TXS(o) ((o) / 4 * (int)sizeof(char *))
#define INPUT_SIZE 80   /* an input record (gPlayers): its pointer at +0x48 is 8 bytes (POINTERS.md 10) */
VIEW(u8[], pause_titlePos);
#define pause_titlePos pause_titlePos_view
VIEW(u8[], gPlayers_save);
#define gPlayers_save gPlayers_save_view
VIEW(u8[], gText);
#define gText gText_view

/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);
void ChangePaneSize(void *,s32,s32,s32,s32);

void pause_TitlePane(void)
{
    s32 x,y,width;
    unsigned char *str;
    unsigned char text[64];
    HAL_SetFont(13);
    str = text;
    if(pause_gControllerRemoved) {
        TB_Sprintf(text,pause_title_format,playerInPause+1,F(F(gText,void *,TXH(4)),char *,TXS(932)));
        width=HAL_GetTextWidth(str,-1);
    } else if(gPauseState==7 || gPauseState==8) {
        width=HAL_GetTextWidth(F(gText,u8 **,TXH(16))[F(F(gText,void *,TXH(12)),u16,112)+gPauseState],-1);
    } else if(gPauseState==6) {
        width=0;
    } else {
        TB_Sprintf(text,pause_title_name_format,(u8 *)*F(gPlayers_save+playerInPause*INPUT_SIZE,void **,0)+PROFILE_OFS(20),F(gText,char **,TXH(16))[F(F(gText,void *,TXH(12)),u16,112)]);
        width=HAL_GetTextWidth(str,-1);
    }
    x=F(pause_titlePos,s16,0);
    y=F(pause_titlePos,s16,2);
    if(!pause_gTitlePane) pause_gTitlePane=InitPane(-20,0,-10,10,176,0,0,0);
    if(!width) {
        ChangePaneSize(pause_gTitlePane,-20,0,-10,10);
    } else {
        ChangePaneSize(pause_gTitlePane,x-width/2-8,y-4,width/2+x+8,TB_GetFontLineHeight()+y+4);
    }
}
