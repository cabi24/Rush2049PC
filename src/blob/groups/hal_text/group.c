#include "game/game.h"

void SetDefaults(u8 *rgba,f32 *amount,s16 *x,s16 *y,s8 *bank,s8 *mode,s8 *variant) {
 rgba[0]=rgba[1]=rgba[2]=230;
 rgba[3]=255;
 *amount=-1.0f;
 *x=0;
 *y=0;
 *bank=-1;
 *mode=-1;
 *variant=-1;
}

/* HAL_FinishText is defined in finishtext.c */
