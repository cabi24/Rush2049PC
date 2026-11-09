#include "game/game.h"

typedef struct tVisual {         /* 0x18 bytes; gGameCar[slot].visuals[] at 0x110 */
    void *next;                  /* a host pointer on the PC */
    s16 index;                   /* 0x04 */
    s16 objnum;                  /* 0x06 */
    s16 slot;                    /* 0x08 */
    s16 pad0A;
    s32 data;                    /* 0x0C */
    f32 timeStamp;               /* 0x10 */
    void (*func)(struct tVisual *, s16);   /* 0x14 */
} tVisual;

typedef struct tCarParts {       /* gCarParts[slot], 0x40 bytes */
    s32 frame;
    u8 pad04[0x18];
    s32 tires[4];                /* 0x1C */
    u8 pad2C[0x14];
} tCarParts;

/* PC: the car palette descriptors car_palettes keep a palette record's N64 layout (24 bytes, data a u32 window
 * address; car_palette_init.c). */
typedef struct Slot24 { u8 flag; u8 other1[15]; u8 active, marker; u16 value; u32 data; } Slot24;

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(tCarParts[], gCarParts);
#define gCarParts gCarParts_view
VIEW(Slot24[], car_palettes);
#define car_palettes car_palettes_view
VIEW(tUVPair, taillight_colors_dim);
#define taillight_colors_dim taillight_colors_dim_view
VIEW(tUVPair, headlight_colors_night);
#define headlight_colors_night headlight_colors_night_view

static void StartHeadLights(s16 slot);
static void StartTire(s16 slot, u32 tire);
static void StartBrakeLights(s16 slot);

void StartCar(s16 slot)
{
    s16 j;
    s32 t;

    StartFrame(slot);
    StartSparks(slot);
    t = gGameState & 8;
    if (t == 0 || (t != 0 && expansion_pak_present != 0)) {
        StartWing(slot, 0);
        StartWing(slot, 1);
        StartWingFlame(slot, 0);
        StartWingFlame(slot, 1);
    }
    if (gRace == 6) StartWeapon(slot);
    StartHeadLights(slot);
    StartBrakeLights(slot);
    for (j = 0; j < 4; j++) StartTire(slot, j);
}

static void StartHeadLights(s16 slot)
{
    u8 *p = car_palettes[slot].data;

    if (gTrackNum == 3 || gTrackNum == 5) {
        ((u16 *)(p + 0x140))[0] = taillight_colors_dim.uv[0];
        ((u16 *)(p + 0x140))[1] = taillight_colors_dim.uv[1];
        ((u16 *)(p + 0x140))[2] = taillight_colors_dim.uv[2];
        ((u16 *)(p + 0x140))[3] = taillight_colors_dim.uv[3];
        ((u16 *)(p + 0x140))[4] = taillight_colors_dim.uv[4];
        ((u16 *)(p + 0x140))[5] = taillight_colors_dim.uv[5];
        ((u16 *)(p + 0x140))[6] = taillight_colors_dim.uv[6];
        ((u16 *)(p + 0x140))[7] = taillight_colors_dim.uv[7];
        ((u16 *)(p + 0x150))[0] = headlight_colors_night.uv[0];
        ((u16 *)(p + 0x150))[1] = headlight_colors_night.uv[1];
        ((u16 *)(p + 0x150))[2] = headlight_colors_night.uv[2];
        ((u16 *)(p + 0x150))[3] = headlight_colors_night.uv[3];
        ((u16 *)(p + 0x150))[4] = headlight_colors_night.uv[4];
        ((u16 *)(p + 0x150))[5] = headlight_colors_night.uv[5];
        ((u16 *)(p + 0x150))[6] = headlight_colors_night.uv[6];
        ((u16 *)(p + 0x150))[7] = headlight_colors_night.uv[7];
    }
}

static void StartTire(s16 slot, u32 tire)
{
    tVisual *v;
    v = &PC_CAR_VISUALS(slot)[tire + 6];
    v->func = AnimateTire;
    v->slot = slot;
    v->data = 0;
    v->index = tire;
    v->timeStamp = gModelTime;
    v->objnum = gCarParts[slot].tires[tire];
}

static void StartBrakeLights(s16 slot)
{
    tVisual *v;
    v = (tVisual *)((uintptr_t)PC_CAR(slot) + 0x110);
    v->func = AnimateBrakeLights;
    v->slot = slot;
}

