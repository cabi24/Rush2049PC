/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* RecordBattle (0x800F497C): per-player pass over active players. For each player i it adds the
 * pairwise signed deltas of gBattlePoints[4][4] into the player's 12-byte record (w8 from row i, wA from |column i|),
 * bumps w4 and, for the leader/flagged players, w6, twice (second pass uses gTotalBattleStats instead of the
 * gPlayers[i].tgt block). Ends with SavePlayerStats(tgt, gTrackNum). Source reconstructed from the retail asm,
 * not from an arcade ancestor. Standalone MATCH; whole-program unit EQUAL (0 locked bodies differ). */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct Tgt { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; u8 **blk; } Tgt;
typedef struct Rec { u8 pad0; u8 idx; u8 pad2[0x46]; Tgt **tgt; } Rec;
typedef struct Acc { u16 w0; u16 w2; u16 w4; u16 w6; u16 w8; u16 wA; } Acc;
extern s16 gNumPlayers;
extern s8 gTrackNum;
extern Rec gPlayers[];
extern void *gJustPlayers[];
extern s8 gRankedPlayer;
extern s8 gCarTeam[];
extern s8 tied[];
extern s8 gBattlePoints[];
extern Acc gTotalBattleStats[];
void SavePlayerStats(void **arg0, u8 arg1);

void RecordBattle(void)
{
    s32 t2;
    s32 i;

    t2 = gTrackNum - 6;
    for (i = 0; i < gNumPlayers; i++)
    {
        if (gPlayers[i].tgt == 0)
        {
            gPlayers[i].tgt = (Tgt **) &gJustPlayers[gPlayers[i].idx];
        }
        if ((*gPlayers[i].tgt)->blk == 0)
        {
            return;
        }
        {
          s32 pass;
          Acc *a2;
          for (pass = 0; pass < 2; pass++)
          {
            s32 k;
            if (pass == 0)
            {
                a2 = (Acc *) ((u8 *) (*((*gPlayers[i].tgt)->blk)) + 12 * t2 + 0x60C);
            }
            else
            {
                a2 = (Acc *) ((u8 *) gTotalBattleStats + 12 * t2);
            }
            for (k = 0; k < gNumPlayers; k++)
            {
                if (k != i)
                {
                    a2->w8 += gBattlePoints[i * 4 + k];
                }
                {
                    s32 v1;
                    s8 v0;
                    v0 = gBattlePoints[k * 4 + i];
                    if (v0 >= 0)
                    {
                        v1 = v0;
                    }
                    else
                    {
                        v1 = -v0;
                    }
                    a2->wA = (u16) (a2->wA + v1);
                }
            }
            a2->w4 = (u16) (a2->w4 + 1);
            if (i == gRankedPlayer || gCarTeam[i] == gCarTeam[gRankedPlayer] || tied[i] == 1)
            {
                a2->w6 = (u16) (a2->w6 + 1);
            }
          }
        }
        SavePlayerStats((void **) gPlayers[i].tgt, gTrackNum);
    }
}
