/* TB_SetTextColor: the text colour by name (MAT3 TB_text.c). */
#include "game/game.h"

/* Makes the text colour entry color of the colour table (kTB_*Text; each entry's second colour is unused here), or for
 * kTB_FlashText the mix of gFlashColor2 and gFlashColor1 by gColorCycle, which then changes every frame: the first
 * flashing text starts TB_TextOncePerFrame (MAT3's TB_TextInit, inline). */
void TB_SetTextColor(int color)
{
 if(color==kTB_FlashText) {
  if(!gInited_TBText) {
   if(!gInited_TBText) {
    gInited_TBText=1;TB_TextOncePerFrame();HAL_InstallOncePerFrameFunc(TB_TextOncePerFrame);
   }
  }
  HAL_SetTextColor(
   (u8)(gFlashColor2.c[0]*(1.0f-gColorCycle)+gFlashColor1.c[0]*gColorCycle),
   (u8)(gFlashColor2.c[1]*(1.0f-gColorCycle)+gFlashColor1.c[1]*gColorCycle),
   (u8)(gFlashColor2.c[2]*(1.0f-gColorCycle)+gFlashColor1.c[2]*gColorCycle),
   (u8)(gFlashColor2.c[3]*(1.0f-gColorCycle)+gFlashColor1.c[3]*gColorCycle));
 } else {
  HAL_SetTextColor(gFontColors[color][0].c[0],gFontColors[color][0].c[1],
   gFontColors[color][0].c[2],gFontColors[color][0].c[3]);
 }
}
