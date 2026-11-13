/* carscr_animNewSlider: the car select screen's sliders (MAT3 carscr.c animNewSlider): the bar beside the tyre size
 * options' value buttons, filled to the size chosen. */
#include "game/game.h"

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    char *Name; void *Image; void *Info;
    s16 TexIndex, X, Y; u16 State; s16 Width, Height;
    u8 Alpha, Flip; s8 Hide; u8 Init;
    s16 Top, Bot, Left, Right, Color; u16 reserved26;
    s32 (*AnimFunc)(struct Blit *); u32 AnimID; intptr_t AnimDTA; u16 BLIdx, reserved36; void *data, *child;
} Blit;

/* One slider blit, each frame. The AnimID packs its type (bits 0-3: 0 the empty bar, 1 the filled part), its player
 * (4-7) and its option (16 up). Hidden for a player not playing (for good), while the car is locked, while the
 * option's button faces away or is invisible. It sits on the option's value button with the button's alpha: a
 * quarter-height strip of the slider texture (the second half while the button shows its selected face, the
 * filled strip a quarter further down), the filled one as wide as the tyre scale (0.75 up, a step a notch, 15
 * notches). Always 1. */
s32 carscr_animNewSlider(Blit *blt)
{
    s32 type=blt->AnimID&15;
    s32 p=(blt->AnimID>>4)&15;
    s32 opt=blt->AnimID>>16;
    s16 screenPos[2];
    if (p>=gNumPlayers) {
        Hidden(blt,1);
        blt->AnimFunc=0;
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
    blt->X=screenPos[0]-blt->Width/2;
    blt->Y=screenPos[1]-(blt->Height/4)/2;
    blt->Left=0;
    blt->Right=blt->Width-1;
    blt->Top=0;
    blt->Bot=(blt->Height/4)-1;
    blt->Alpha=carscr_buttonInfo[p][opt].alpha;
    if (carscr_slider_flip_angle < carscr_buttonInfo[p][opt].angle) {
        blt->Top += blt->Height/2;
        blt->Bot += blt->Height/2;
    }
    if (type==SLIDER_FILLED) {
        blt->Top+=(blt->Height/4);
        blt->Bot+=(blt->Height/4);
        switch (opt) {
        case CAR_OPT_FRONT_TIRE_SCALE:
            blt->Right=(gFrontTireScale[p + 1][gThisCar[p]]-.75f)/carscr_front_tire_scale_step*(blt->Width-1)/15.0f;
            break;
        case CAR_OPT_REAR_TIRE_SCALE:
            blt->Right=(gRearTireScale[p + 1][gThisCar[p]]-.75f)/carscr_rear_tire_scale_step*(blt->Width-1)/15.0f;
            break;
        }
    }
    UpdateBlit(blt);
    return 1;
}
