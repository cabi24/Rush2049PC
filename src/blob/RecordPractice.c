/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* RecordPractice (0x800F54C0): per active player, adds the car's dist/528 to the u32 sort key at +0x58 of
 * the player's two 96-byte records (the gPlayers[i].tgt block, then gTotalRaceStats), then calls SavePlayerStats
 * like its sibling RecordBattle. Not the arcade RecordPractice (which sorts by place).
 * built the walk (named `cp` car base, (u8) call argument); the last 8 words were the
 * FP temp ring: the sum and the float->u32 conversion are one expression (no named f32 local), so ugen's ring
 * assigns $f18/$f4 as in retail. No shaping devices. */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Tgt { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; u8 **blk; } Tgt;
typedef struct Rec { u8 b0; u8 idx; u8 pad2[0x46]; Tgt **tgt; } Rec;
typedef struct Car { u8 pad[264]; f32 dist; u8 pad2[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 pad2_308[0x380 - 0x308]; void *input; u8 pad2_384[0x3B8 - 0x384]; } Car;
extern s16 gNumPlayers;
extern s8 gTrackNum;
extern s8 gBackwards;
extern Rec gPlayers[];
extern void *gJustPlayers[];
extern u8 gTotalRaceStats[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
void SavePlayerStats(void **arg0, u8 arg1);

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
