/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/*
 * Return whether any active four-byte row has a nonzero selected byte.
 * Native count is signed 16-bit; the field index is a full signed word.
 * N64-side table scan. No direct arcade equivalent is established because the
 * primary arcade source checkout is unavailable. No bounds checks are added.
 */
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

extern s16 gNumPlayers;
extern s8 gUnlockedCircuits[][4];

s32 IsCircuitUnlocked(s32 field)
{
    s32 found = 0;
    s32 i;

    for (i = 0; i < gNumPlayers; i++) {
        if (gUnlockedCircuits[i][field]) {
            found = 1;
        }
    }
    return found;
}
