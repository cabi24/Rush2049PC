/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * StartCar (0x800B0C48, 600 B) = MAT3 StartCar@visuals.c (GC/DC names; map row), N64 version:
 *   StartFrame, StartSparks, StartWing x2 (StartWing, IPA s1/s2) and
 *   StartWingFlame x2 (StartWingFlame, IPA t0/t3) unless demo flag 8 is set without expansion_pak_present, StartWeapon
 *   (image B 0x8038C910) when gRace == 6, then the inlined statics StartHeadLights (N64: copy the two 8 x u16
 *   head-light UV blocks taillight_colors_dim / headlight_colors_night into the car model data at +0x140 / +0x150 on tracks 3 and 5),
 *   StartBrakeLights (visuals[0]: AnimateBrakeLights AnimateBrakeLights, slot) and the four StartTire calls
 *   (visuals[tire + 6]: AnimateTire AnimateTire, slot, data 0, index, gModelTime, gCarParts[slot].tires[tire]).
 * Levers (from the draft at 25-27 words): MAT3 structure with static helpers; the helpers are
 *   defined AFTER StartCar (MAT3 file order: StartCar 615 < StartTire 1706 < StartBrakeLights 2323): as1 breaks the
 *   ready-list tie between the brake-light stores and the hoisted tire-loop invariants (which carry the for-loop
 *   line) by source line (6 words with the helpers first).  AnimateTire / AnimateBrakeLights / headlight_colors_night are
 *   referenced only here (no out-of-line copies), so the helpers are deleted statics; their stubs land before
 *   StartWingFlame in the group object, as retail's four stubs visuals_stub_a..F4 do (the fourth is another static).
 * DISCLOSED SHAPING: StartBrakeLights computes its visual as `(tVisual *)((u32)&gGameCar[slot] + 0x110)`: retail
 *   shares `&gGameCar[slot]` (a2) with the tire loop and keeps visuals[0] as a2 + 272 in v0; `&gGameCar[slot].visuals[0]`,
 *   `.visuals`, a car pointer local, a (u8 *) offset or an inline getter all fold the 272 into the store offsets and
 *   recompute slot * 952 (26-32 words).  The integer form is the only one found.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

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
typedef struct tCarData {        /* gGameCar[slot], 0x3B8 bytes */
    u8 pad000[0x110];
    tVisual visuals[20];         /* 0x110 */
    struct pc_visual pad2F0[1]; u8 pad2F0_308[0x380 - 0x308]; void *input; u8 pad2F0_384[0x3B8 - 0x384];
} tCarData;
typedef struct tCarParts {       /* gCarParts[slot], 0x40 bytes */
    s32 frame;
    u8 pad04[0x18];
    s32 tires[4];                /* 0x1C */
    u8 pad2C[0x14];
} tCarParts;
typedef struct { u16 uv[8]; } tUVPair;
/* PC: the car palette descriptors car_palettes keep a palette record's N64 layout (24 bytes, data a u32 window
 * address; car_palette_init.c). */
typedef struct Slot24 { u8 flag; u8 other1[15]; u8 active, marker; u16 value; u32 data; } Slot24;

extern s32 gGameState;
extern s32 gRace;
extern s8 expansion_pak_present;
extern s8 gTrackNum;
extern f32 gModelTime;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern tCarParts gCarParts[];   /* gCarParts */
extern Slot24 car_palettes[];
extern tUVPair taillight_colors_dim, headlight_colors_night;

void StartFrame(s16 slot);
void StartSparks(s16 slot);
void StartWingFlame(s16 slot, s16 side);
void StartWing(s16 slot, s16 side);
void StartWeapon(s16 slot);    /* image B */
void AnimateBrakeLights(tVisual *v, s16 op);
void AnimateTire(tVisual *v, s16 op);

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

