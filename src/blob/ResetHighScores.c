/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ResetHighScores (0x800F7EB0): reset routine. For the first gNumPlayers entries, clear the byte array
 * gLap and set the float array gBestLapTime to 5999.999f; then set all fifteen bytes of the
 * three 5-byte records at gNewTotalScore to -1. Also matches at -O2.
 *
 * The float is the function's own .rodata literal (0x80124618 = 0x45BB7FFE). Earlier research
 * (tiny_A47) spelled it `extern float D_80124618`, which is a different
 * symbol to as1 and left 4 words in a different (dependency-valid) order; the natural literal closes it.
 */
extern short gNumPlayers;
extern float gBestLapTime[];
extern unsigned char gLap[];
extern signed char gNewTotalScore[3][5];

void ResetHighScores(void)
{
    int i, j;

    for (i = 0; i < gNumPlayers; i++) {
        gLap[i] = 0;
        gBestLapTime[i] = 5999.999f;
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 5; j++) {
            gNewTotalScore[i][j] = -1;
        }
    }
}
