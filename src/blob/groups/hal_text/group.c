/* SetDefaults: the text state at the start of a frame (MAT3 HAL_text.c's _SetDefaults). */
#include "game/game.h"

/* Sets a text state to the defaults: light grey (230, 230, 230), opaque (translucency -1), at (0, 0), no font, the
 * font's own leading and spacing (-1). HAL_StartText and HAL_FinishText reset the current state with it,
 * HAL_DrawText the state its buffer last recorded. */
void SetDefaults(u8 *fontColor,f32 *trans,s16 *fontXpos,s16 *fontYpos,s8 *fontIndex,s8 *leading,s8 *spacing) {
 fontColor[0]=fontColor[1]=fontColor[2]=230;
 fontColor[3]=255;
 *trans=-1.0f;
 *fontXpos=0;
 *fontYpos=0;
 *fontIndex=-1;
 *leading=-1;
 *spacing=-1;
}

/* HAL_FinishText is defined in finishtext.c */
