/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
/* PC: the visual (tVisual, pc_prelude.h's struct pc_visual) has a host-sized link: its car slot is at +0xC, where
 * +8 is its index (docs/port/LAYOUT.md). */
typedef struct { void *next; s16 index, objnum; s16 idx; } In;
typedef struct { u8 pad[320]; u16 v[8]; } Out;
/* PC: a car record seen from +0xE8 (a name inside gGameCar): only its stride, PC_CAR_SIZE, matters here. */
typedef struct { u32 flags; u8 pad[PC_CAR_SIZE - 4]; } PF;
/* PC: the car palette descriptors car_palettes keep a palette record's N64 layout (24 bytes, data a u32 window
 * address; car_palette_init.c). */
typedef struct Slot24 { u8 flag; u8 other1[15]; u8 active, marker; u16 value; u32 data; } Slot24;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Slot24 car_palettes[];
extern s8 gTrackNum;
extern u16 taillight_colors_dark[8];
extern u16 taillight_colors_dim[8];
extern u16 taillight_colors_bright[8];
#define CP(src) do { \
    o->v[0] = (src)[0]; o->v[1] = (src)[1]; o->v[2] = (src)[2]; o->v[3] = (src)[3]; \
    o->v[4] = (src)[4]; o->v[5] = (src)[5]; o->v[6] = (src)[6]; o->v[7] = (src)[7]; } while (0)

/* The descriptor's data is a 32-bit N64 address: N64PTR0(T, a) is the pointer at one (pc_prelude.h, docs/port/NOFIXED.md
 * 2.3). */
#define N64PTR0(T, a) ((T)PC_N64P(a))
void AnimateBrakeLights(In *in, s16 flag)
{
s32 i; Out *o; s32 ok;
 i = in->idx;
 ok = (PC_CAR_FLAGS(i) & 0x1000) != 0 && (PC_CAR_FLAGS(i) & 0x10) == 0;
 o = N64PTR0(Out *, car_palettes[i].data);
    if (flag == 0 || !ok) {
        if (gTrackNum == 3 || gTrackNum == 5) { CP(taillight_colors_dim); } else { CP(taillight_colors_dark); }
    } else {
        if (gTrackNum == 3 || gTrackNum == 5) { CP(taillight_colors_bright); } else { CP(taillight_colors_dim); }
    }
}