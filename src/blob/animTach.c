/* animTach: the tachometer, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

#define TACH_DIAL 0     /* animID's low nibble: the dial (the texture's top half) */
#define TACH_NEEDLE 1   /* the bar over it (the bottom half), as long as the engine's speed */

/* The tachometer of the player in animID's high nibble, part TACH_DIAL or TACH_NEEDLE (low nibble): hidden when the
 * HUD hides it, the car shifts by itself or has finished; the medium or small texture for 2 or more windows; placed
 * at tachPos; the bar cut at the engine's speed (rpm * 1.35 / 10000 of the width, at most all). A player past the
 * windows hides it, and past the players stops animating it. Returns 1. */
s32 animTach(tBlit *blt)
{
    s32 slot, kind, hide;
    f32 amount;
    slot=(blt->animID & 0xf0)>>4;
    kind=blt->animID & 0xf;
    if(slot>=gNumWindows) {
        if(slot>=gNumPlayers) blt->animFunc=0;
        if (blt->hide != 1) {
            blt->hide = 1;
            UpdateBlit(blt);
        }
        return 1;
    }
    hide = gShowTach == 0 || PC_MODEL_AUTOTRANS(slot) != 0 ||
           PC_CAR_PLACE_LOCKED(PC_MODEL_SLOT(slot)) == 1;
    if (hide != blt->hide) {
        blt->hide = hide;
        UpdateBlit(blt);
    }
    if (blt->hide) return 1;
    if(gNumWindows==2) RenameBlit(blt,tachometer_md_name,0);
    else if(gNumWindows>=3) RenameBlit(blt,tachometer_sm_name,0);
    blt->x=tachPos[gNumWindows-1][slot][0]-blt->width/2;
    blt->y=tachPos[gNumWindows-1][slot][1];
    if(kind==TACH_DIAL) blt->bot=blt->height/2-1;
    else if(kind==TACH_NEEDLE) {
        amount=(PC_MODEL_RPM(slot)*1.35f)*0.0001f;
        if(amount<0.0f) amount=-amount;
        if(amount>1.0f) amount=1.0f;
        blt->right=(s32)((blt->width-1)*amount);
        blt->top=blt->height/2;
        blt->bot=blt->height-1;
    }
    blt->alpha=254;
    UpdateBlit(blt);
    return 1;
}
