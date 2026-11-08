/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef int s32; typedef float f32;
typedef struct { u8 b0; u8 id; u8 pad[6]; } Slot8;
typedef struct { s32 a; s8 b; s8 c; u8 pad[6]; } SelectCarWave;
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern SelectCarWave gSelectCarWavePtrs[];

void InitSelectCarLoad(void)
{
    s32 i;

    for (i = 0; i < 13; i++) {
        PC_LINK_CAR(i) = i;
    }
    for (i = 0; i < 16; i++) {
        gSelectCarWavePtrs[i].a = -1;
        gSelectCarWavePtrs[i].c = -1;
        gSelectCarWavePtrs[i].b = 0;
    }
}
