/* anim_car_color_marker: the car select screen's colour swatches (N64 only; MAT3 has paint jobs instead): a small
 * square beside each of the three colour options' value buttons, in the colour chosen. */
#include "game/game.h"

/* One swatch blit, each frame. The animID packs its player (bits 0-3), its option (16-23) and, written here, the
 * palette entry shown (28-31). Hidden for a player not playing (for good), while the car is locked, or while the
 * option's button faces away or is invisible. It sits on the option's value button with the button's alpha, and is
 * a flat square (a blit whose palette is a colour) of the chosen colour (paint_colors_rgb), kept in
 * carscr_marker_colors[player][channel]. Always 1. */
s32 anim_car_color_marker(tBlit *blt)
{
    s8 (*color_a)[13] = car_color_choice_a;
    s8 (*color_b)[13] = car_color_choice_b;
    s8 (*color_c)[13] = car_color_choice_c;
    s32 player=blt->animID&15;
    s32 marker=(blt->animID>>16)&255;
    MenuButton *slot;
    s32 palette;
    s16 position[2];
    s32 channel;

    if (player>=gNumPlayers) {
        Hidden(blt,1);
        blt->animFunc=0;
        return 1;
    }
    if (gCarLocked[player]) {
        Hidden(blt,1);
        return 1;
    }
    slot=&carscr_buttonInfo[player][marker];
    if (Hidden(blt,
        (carscr_marker_hide_angle_min<slot->angle && slot->angle<carscr_marker_hide_angle_max) || !slot->alpha))
        return 1;
    ZOID_ProjectWindow(player,slot[CAR_OPTS].position,&gCamera[player],0,position);
    blt->x=position[0]-blt->width/2;
    blt->y=position[1]-blt->height/2;
    blt->alpha=slot->alpha;
    /* Original game: palette and channel are left unset for an option other than the three colours, so such a
     * blit would read and write through uninitialised indices (docs/code/overlays.md). */
    if (marker==CAR_OPT_COLOR_A) {
        palette=color_a[player + 1][gThisCar[player]];
        channel=0;
    } else if (marker==CAR_OPT_COLOR_B) {
        palette=color_b[player + 1][gThisCar[player]];
        channel=1;
    } else if (marker==CAR_OPT_COLOR_C) {
        palette=color_c[player + 1][gThisCar[player]];
        channel=2;
    }
    carscr_marker_colors[player][channel].c[0]=paint_colors_rgb[palette].c[0];
    carscr_marker_colors[player][channel].c[1]=paint_colors_rgb[palette].c[1];
    carscr_marker_colors[player][channel].c[2]=paint_colors_rgb[palette].c[2];
    carscr_marker_colors[player][channel].c[3]=blt->alpha;
    blt->palette=&carscr_marker_colors[player][channel];
    blt->animID=(blt->animID&0x0fffffff)|((u32)palette<<28);
    UpdateBlit(blt);
    return 1;
}
