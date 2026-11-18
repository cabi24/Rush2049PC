/* carscr_animNewSlider: the car select screen's sliders (MAT3 carscr.c animNewSlider): the bar beside the tyre size
 * options' value buttons, filled to the size chosen. */
#include "game/game.h"

/* One slider blit, each frame. The animID packs its type (bits 0-3: 0 the empty bar, 1 the filled part), its player
 * (4-7) and its option (16 up). Hidden for a player not playing (for good), while the car is locked, while the
 * option's button faces away or is invisible. It sits on the option's value button with the button's alpha: a
 * quarter-height strip of the slider texture (the second half while the button shows its selected face, the
 * filled strip a quarter further down), the filled one as wide as the tyre scale (0.75 up, a step a notch, 15
 * notches). Always 1. */
s32 carscr_animNewSlider(tBlit *blt)
{
    s32 type=blt->animID&15;
    s32 p=(blt->animID>>4)&15;
    s32 opt=blt->animID>>16;
    s16 screenPos[2];
    if (p>=gNumPlayers) {
        Hidden(blt,1);
        blt->animFunc=0;
        return 1;
    }
    if (gCarLocked[p]) {
        Hidden(blt,1);
        return 1;
    }
    if (Hidden(blt,
        (carscr_slider_hide_angle_min<carscr_buttonInfo[p][opt].angle && carscr_buttonInfo[p][opt].angle<carscr_slider_hide_angle_max) || !carscr_buttonInfo[p][opt].alpha))
        return 1;
    ZOID_ProjectWindow(p,carscr_buttonInfo[p][opt+CAR_OPTS].position,&gCamera[p],0,screenPos);
    blt->x=screenPos[0]-blt->width/2;
    blt->y=screenPos[1]-(blt->height/4)/2;
    blt->left=0;
    blt->right=blt->width-1;
    blt->top=0;
    blt->bot=(blt->height/4)-1;
    blt->alpha=carscr_buttonInfo[p][opt].alpha;
    if (carscr_slider_flip_angle < carscr_buttonInfo[p][opt].angle) {
        blt->top += blt->height/2;
        blt->bot += blt->height/2;
    }
    if (type==SLIDER_FILLED) {
        blt->top+=(blt->height/4);
        blt->bot+=(blt->height/4);
        switch (opt) {
        case CAR_OPT_FRONT_TIRE_SCALE:
            blt->right=(gFrontTireScale[p + 1][gThisCar[p]]-.75f)/carscr_front_tire_scale_step*(blt->width-1)/15.0f;
            break;
        case CAR_OPT_REAR_TIRE_SCALE:
            blt->right=(gRearTireScale[p + 1][gThisCar[p]]-.75f)/carscr_rear_tire_scale_step*(blt->width-1)/15.0f;
            break;
        }
    }
    UpdateBlit(blt);
    return 1;
}
