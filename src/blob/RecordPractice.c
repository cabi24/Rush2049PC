#include "game/game.h"

typedef struct Tgt { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; u8 **blk; } Tgt;
typedef struct Rec { u8 b0; u8 idx; u8 pad2[0x46]; Tgt **tgt; } Rec;
typedef struct Car { u8 pad[264]; f32 dist; u8 pad2[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 pad2_308[0x380 - 0x308]; void *input; u8 pad2_384[0x3B8 - 0x384]; } Car;

extern Rec gPlayers[];
VIEW(void *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view
VIEW(u8[], gTotalRaceStats);
#define gTotalRaceStats gTotalRaceStats_view

void RecordPractice(void)
{
    s32 i;
    s32 s6;
    s32 s7;

    s6 = gTrackNum;
    s7 = gTrackNum;
    if (gBackwards)
    {
        s6 += 6;
        s7 += 19;
    }
    for (i = 0; i < gNumPlayers; i++)
    {
        s32 pass;
        Car *cp;
        if (gPlayers[i].tgt == 0)
        {
            gPlayers[i].tgt = (Tgt **) &gJustPlayers[gPlayers[i].idx];
        }
        if ((*gPlayers[i].tgt)->blk == 0)
        {
            return;
        }
        for (pass = 0; pass < 2; pass++)
        {
            u8 *p;
            if (pass == 0)
            {
                p = *((*gPlayers[i].tgt)->blk) + s6 * 96 + 140;
            }
            else
            {
                p = gTotalRaceStats + s6 * 96;
            }
            cp = PC_CAR(gPlayers[i].b0);
            *((u32 *) (p + 88)) = (u32) ((f32) *((u32 *) (p + 88)) + cp->dist / 528.0f);
        }
        SavePlayerStats((void **) gPlayers[i].tgt, (u8) s7);
    }
}
