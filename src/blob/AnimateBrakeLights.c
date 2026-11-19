/* AnimateBrakeLights.c: a car's brake lights (MAT3 visuals.c, AnimateBrakeLights()).
 *
 * The lights are colours in the car's palette (the TLUT SetupCarColor made, car_palettes[slot]), not objects: the
 * animator rewrites the eight tail-light entries each frame. */
#include "game/game.h"

#define CP(src) do { \
    tlut->taillights[0] = (src)[0]; tlut->taillights[1] = (src)[1]; tlut->taillights[2] = (src)[2]; tlut->taillights[3] = (src)[3]; \
    tlut->taillights[4] = (src)[4]; tlut->taillights[5] = (src)[5]; tlut->taillights[6] = (src)[6]; tlut->taillights[7] = (src)[7]; } while (0)

/* The brake-light visual of car v->slot (VIS_BRAKE_LIGHTS): bright tail lights while it brakes (APPEAR_BRAKE) and is
 * not a hulk, else the plain ones; op 0 (stop) puts the plain ones back. On the night tracks (3 and 5) the lights
 * are on, so plain is dim and braking bright; elsewhere plain is dark and braking dim. */
void AnimateBrakeLights(tVisual *v, s16 op)
{
s32 slot; CarTlut *tlut; s32 braking;
 slot = v->slot;
 braking = (pc_cars[slot].appearance & APPEAR_BRAKE) != 0 && (pc_cars[slot].appearance & APPEAR_HULK) == 0;
 tlut = car_palettes[slot].palBase;
    if (op == 0 || !braking) {
        if (gTrackNum == 3 || gTrackNum == 5) { CP(taillight_colors_dim.uv); } else { CP(taillight_colors_dark); }
    } else {
        if (gTrackNum == 3 || gTrackNum == 5) { CP(taillight_colors_bright); } else { CP(taillight_colors_dim.uv); }
    }
}
