/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Record a lap split for a player (N64 race code; no arcade ancestor found).
 * Unless the debug flag bit 3 of gGameState is set or the mode word
 * gRace is 1, and only for player < gNumPlayers (active count):
 * the per-player lap counter gLap[player] (u8) selects the next slot of
 * the eight-float row gLapTimes[player]; the cumulative time is stored there
 * and every previous lap time is subtracted from it, giving the lap time.
 * The best-lap table gBestLapTime[player] is lowered when beaten and the lap
 * counter incremented.
 *
 * Shaping quirk: every access spells the counter as gLap[player] (no
 * local copy, no pointer to the slot). With a local count or slot pointer uopt
 * unrolls the subtraction loop by 4 (76 words); with the global spelling the
 * float stores into gLapTimes provably miss gLap, so the bound is
 * hoisted after the unrolling decision and the loop stays rolled.
 */
typedef float f32;
typedef short s16;
typedef unsigned char u8;

extern int gGameState;
extern int gRace;
extern s16 gNumPlayers;
extern f32 gLapTimes[][8];
extern u8 gLap[];
extern f32 gBestLapTime[];

void RecordLap(int player, f32 time)
{
    int i;

    if (!(gGameState & 8) && gRace != 1 && player < gNumPlayers) {
        gLapTimes[player][gLap[player]] = time;
        for (i = 0; i < gLap[player]; i++) {
            gLapTimes[player][gLap[player]] -= gLapTimes[player][i];
        }
        if (gLapTimes[player][gLap[player]] < gBestLapTime[player]) {
            gBestLapTime[player] = gLapTimes[player][gLap[player]];
        }
        gLap[player]++;
    }
}
