/* The audio options screen's menu size (MAT3 audioscr.c's calcMenuRect). */
#include "game/game.h"


/* Sizes the menu box (audio_menuRect) for its widest line name and widest value (a volume, a song name in
 * capitals, mono or stereo), centred on the screen; cx is where the values start. */
void audio_calcMenuRect(void)
{
    s32 menuIdx,u0,menuCount,optionWidth,valueWidth;
    u8 str[40];
    u8 *charPtr;
    s32 songIdx;
    HAL_SetFont(10);
    optionWidth=0;
    valueWidth=0;
    menuCount=0;
    for(menuIdx=0;menuIdx<4;menuIdx++) {
        optionWidth = HAL_GetTextWidth(gText.taba[gText.tab[39]+menuIdx],-1)>optionWidth ? HAL_GetTextWidth(gText.taba[gText.tab[39]+menuIdx],-1):optionWidth;
        if(audio_validOption(menuIdx)) {
            switch(menuIdx) {
            case 0: case 1:
                valueWidth=HAL_GetTextWidth("10",-1)>valueWidth ? HAL_GetTextWidth("10",-1):valueWidth;
                break;
            case 2:
                for(songIdx=0;songIdx<14;songIdx++) {
                    if(songIdx<12) TB_Strcpy(str,gSongNames[songIdx]);
                    else if(songIdx==12) TB_Strcpy(str,gText.single[34]);
                    else if(songIdx==13) TB_Strcpy(str,gText.taba[gText.tab[16]]);
                    for(charPtr=str;*charPtr != 0;charPtr++)
                        *charPtr=(*charPtr>='a' && *charPtr<='z') ? *charPtr^0x20:*charPtr;
                    valueWidth=HAL_GetTextWidth(str,-1)>valueWidth ? HAL_GetTextWidth(str,-1):valueWidth;
                }
                break;
            case 3:
                valueWidth=HAL_GetTextWidth(gText.taba[gText.tab[40]],-1)>valueWidth ? HAL_GetTextWidth(gText.taba[gText.tab[40]],-1):valueWidth;
                valueWidth=HAL_GetTextWidth(gText.taba[gText.tab[40]+1],-1)>valueWidth ? HAL_GetTextWidth(gText.taba[gText.tab[40]+1],-1):valueWidth;
                break;
            }
            menuCount++;
        }
    }
    audio_menuRect.w=optionWidth+valueWidth+10;
    audio_menuRect.h=menuCount*TB_GetFontLineHeight();
    audio_menuRect.x=160-audio_menuRect.w/2;
    audio_menuRect.cx=audio_menuRect.x+optionWidth+5;
}
