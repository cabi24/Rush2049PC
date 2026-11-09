/* AnimateBrakeLights.c: a car's brake lights (MAT3 visuals.c, AnimateBrakeLights()).
 *
 * The lights are colours in the car's palette (the TLUT SetupCarColor made, car_palettes[slot]), not objects: the
 * animator rewrites the eight tail-light entries each frame. */
#include "game/game.h"

/* A car visual (tVisual) on the PC: next a host pointer (docs/port/CARS.md 3). */
typedef struct tVisual { void *next; s16 index, objnum; s16 slot; } tVisual;
/* The car's palette (256 RGBA 5551 entries): the tail lights are entries 160-167, the headlights 168-175. */
typedef struct CarTlut { u8 paint[320]; u16 taillights[8]; } CarTlut;

/* PC: the car palette descriptors car_palettes (CarPalette) keep a palette record's N64 layout (24 bytes, data a
 * u32 window address; car_palette_init.c). */

VIEW(u16[8], taillight_colors_dark);
#define taillight_colors_dark taillight_colors_dark_view
VIEW(u16[8], taillight_colors_dim);
#define taillight_colors_dim taillight_colors_dim_view
VIEW(u16[8], taillight_colors_bright);
#define taillight_colors_bright taillight_colors_bright_view
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
 braking = (PC_CAR_FLAGS(slot) & APPEAR_BRAKE) != 0 && (PC_CAR_FLAGS(slot) & APPEAR_HULK) == 0;
 tlut = ((CarTlut *)PC_N64P(car_palettes[slot].data));
    if (op == 0 || !braking) {
        if (gTrackNum == 3 || gTrackNum == 5) { CP(taillight_colors_dim); } else { CP(taillight_colors_dark); }
    } else {
        if (gTrackNum == 3 || gTrackNum == 5) { CP(taillight_colors_bright); } else { CP(taillight_colors_dim); }
    }
}
