/* trackscr_animNewSlider: the track select screen's sliders (MAT3 trackscr.c animNewSlider): the bars beside the
 * fog, wind, difficulty and catch-up options' value buttons, filled to the setting. */
#include "game/game.h"

typedef struct Blit {
    char *Name; void *Image; void *Info;
    s16 TexIndex, X, Y; u16 State; s16 Width, Height;
    u8 Alpha, Flip; s8 Hide; u8 Init;
    s16 Top, Bot, Left, Right, Color; u16 reserved26;
    s32 (*AnimFunc)(struct Blit *); u32 AnimID;
} Blit;

/* One slider blit, each frame. The AnimID packs its segment (bits 0-3: 0 the empty bar, 1 the filled part) and its
 * option (16 up). Hidden while the option's value button faces away (between 40 and 140 degrees) or is invisible.
 * It sits on the value button with the button's alpha (254 at most): a quarter-height strip of the slider texture
 * (the second half while the button shows its selected face, the filled strip a quarter further down), the filled
 * one as wide as the setting's share of its range (fog 0-3, wind 0-4, difficulty 0-5, catch-up 0-2). Always 1. */
s32 trackscr_animNewSlider(Blit *blt)
{
    s32 segment=blt->AnimID&15;
    s32 marker=blt->AnimID>>16;
    s16 position[2];

    if (Hidden(blt,
        (0.6981316805f<trackscr_buttonInfo[marker+TRACK_OPTS].angle && trackscr_buttonInfo[marker+TRACK_OPTS].angle<2.443460941f)
        || !trackscr_buttonInfo[marker+TRACK_OPTS].alpha))
        return 1;
    ZOID_ProjectWindow(0,trackscr_buttonInfo[marker+TRACK_OPTS].position,&gCamera[0],0,position);
    blt->X=position[0]-blt->Width/2;
    blt->Y=position[1]-(blt->Height/4)/2;
    blt->Top=0;
    blt->Bot=blt->Height/4-1;
    blt->Left=0;
    blt->Right=blt->Width-1;
    blt->Alpha=trackscr_buttonInfo[marker+TRACK_OPTS].alpha;
    if (blt->Alpha==255)
        blt->Alpha=254;
    if (1.570796371f<trackscr_buttonInfo[marker+TRACK_OPTS].angle) {
        blt->Top+=blt->Height/2;
        blt->Bot+=blt->Height/2;
    }
    if (segment==SLIDER_FILLED) {
        blt->Top+=blt->Height/4;
        blt->Bot+=blt->Height/4;
        switch (marker) {
        case TRACK_OPT_FOG: blt->Right=(blt->Width-1)*gFogIntensity/3; break;
        case TRACK_OPT_WIND: blt->Right=(blt->Width-1)*gWind/4; break;
        case TRACK_OPT_DIFFICULTY: blt->Right=(blt->Width-1)*gDifficulty/5; break;
        case TRACK_OPT_CATCHUP: blt->Right=(blt->Width-1)*gCatchup/2; break;
        }
    }
    UpdateBlit(blt);
    return 1;
}
