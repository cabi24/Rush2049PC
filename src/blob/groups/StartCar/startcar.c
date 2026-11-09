/* startcar.c: start a car's visuals (MAT3 visuals.c and headlights.c: StartCar(), StartHeadLights(), StartTire(),
 * StartBrakeLights()). */
#include "game/game.h"

/* The car's palette (256 RGBA 5551 entries): the tail lights are entries 160-167, the headlights 168-175. */
typedef struct CarTlut { u8 paint[320]; u16 taillights[8]; u16 headlights[8]; } CarTlut;

/* PC: the car palette descriptors car_palettes (CarPalette) keep a palette record's N64 layout (24 bytes, data a
 * u32 window address; car_palette_init.c). */

static void StartHeadLights(s16 slot);
static void StartTire(s16 slot, u32 tire);
static void StartBrakeLights(s16 slot);

/* Start car `slot`'s visuals: the frame, the sparks, the wings and their flames (not in the attract demo, game
 * state 8 GS_DEMO, unless the Expansion Pak is there), the weapon in a battle (gRace 6, kBattle), the head and
 * brake lights and the four tires. MAT3 has the wings in the demo too. */
void StartCar(s16 slot)
{
    s16 j;
    s32 demo;

    StartFrame(slot);
    StartSparks(slot);
    demo = gGameState & GS_DEMO;
    if (demo == 0 || (demo != 0 && expansion_pak_present != 0)) {
        StartWing(slot, 0);
        StartWing(slot, 1);
        StartWingFlame(slot, 0);
        StartWingFlame(slot, 1);
    }
    if (gRace == kBattle) StartWeapon(slot);
    StartHeadLights(slot);
    StartBrakeLights(slot);
    for (j = 0; j < 4; j++) StartTire(slot, j);
}

/* On the night tracks (3 and 5) turn car `slot`'s lights on: the tail lights dim and the headlights lit, in its
 * palette. MAT3 makes headlight objects and a visual instead (headlights.c).
 * PC: the descriptor's data is a u32 window address: the palette is at PC_N64P of it (as in AnimateBrakeLights). */
static void StartHeadLights(s16 slot)
{
    CarTlut *tlut = (CarTlut *) PC_N64P(car_palettes[slot].data);

    if (gTrackNum == 3 || gTrackNum == 5) {
        tlut->taillights[0] = taillight_colors_dim.uv[0];
        tlut->taillights[1] = taillight_colors_dim.uv[1];
        tlut->taillights[2] = taillight_colors_dim.uv[2];
        tlut->taillights[3] = taillight_colors_dim.uv[3];
        tlut->taillights[4] = taillight_colors_dim.uv[4];
        tlut->taillights[5] = taillight_colors_dim.uv[5];
        tlut->taillights[6] = taillight_colors_dim.uv[6];
        tlut->taillights[7] = taillight_colors_dim.uv[7];
        tlut->headlights[0] = headlight_colors_night.uv[0];
        tlut->headlights[1] = headlight_colors_night.uv[1];
        tlut->headlights[2] = headlight_colors_night.uv[2];
        tlut->headlights[3] = headlight_colors_night.uv[3];
        tlut->headlights[4] = headlight_colors_night.uv[4];
        tlut->headlights[5] = headlight_colors_night.uv[5];
        tlut->headlights[6] = headlight_colors_night.uv[6];
        tlut->headlights[7] = headlight_colors_night.uv[7];
    }
}

/* Start the visual of car `slot`'s tire `tire` (0-3, VIS_TIRES + tire): AnimateTire turns, steers and bounces the
 * tire object; its data holds the tire's turned angle, from 0. */
static void StartTire(s16 slot, u32 tire)
{
    struct pc_visual *v;
    v = &PC_CAR_VISUALS(slot)[tire + VIS_TIRES];
    v->func = AnimateTire;
    v->slot = slot;
    v->data = 0;
    v->index = tire;
    v->timeStamp = gModelTime;
    v->objnum = gCarParts[slot].tires[tire];
}

/* Start car `slot`'s brake-light visual (VIS_BRAKE_LIGHTS, AnimateBrakeLights). */
static void StartBrakeLights(s16 slot)
{
    struct pc_visual *v;
    v = (struct pc_visual *)((uintptr_t)PC_CAR(slot) + 0x110);
    v->func = AnimateBrakeLights;
    v->slot = slot;
}
