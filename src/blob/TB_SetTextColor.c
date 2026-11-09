/* TB_SetTextColor: the text colour by name (MAT3 TB_text.c). */
#include "game/game.h"
#define Color8 Color8View   /* this file's own Color8: include/game's Color8 is another type */

typedef struct Color4 {u8 r,g,b,a;} Color4;
typedef struct Color8 {Color4 color;u8 extra[4];} Color8;

VIEW(Color4, gFlashColor2);
#define gFlashColor2 gFlashColor2_view
VIEW(Color4, gFlashColor1);
#define gFlashColor1 gFlashColor1_view

VIEW(Color8[], gFontColors);
#define gFontColors gFontColors_view

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
   (u8)(gFlashColor2.r*(1.0f-gColorCycle)+gFlashColor1.r*gColorCycle),
   (u8)(gFlashColor2.g*(1.0f-gColorCycle)+gFlashColor1.g*gColorCycle),
   (u8)(gFlashColor2.b*(1.0f-gColorCycle)+gFlashColor1.b*gColorCycle),
   (u8)(gFlashColor2.a*(1.0f-gColorCycle)+gFlashColor1.a*gColorCycle));
 } else {
  HAL_SetTextColor(gFontColors[color].color.r,gFontColors[color].color.g,
   gFontColors[color].color.b,gFontColors[color].color.a);
 }
}
