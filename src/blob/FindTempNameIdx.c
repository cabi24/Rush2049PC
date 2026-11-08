/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Name-cache slot allocator: 20 slots of 13-byte names at 0x80151968 with a
 * u16 age per slot at 0x80151A78.  Ages every slot, then returns the slot
 * that already holds `name` (game_strcmp is strcmp), else the first empty
 * slot, else the first slot not referenced by the 12x3x5 table of slot
 * numbers at 0x80151690, else the oldest slot (every reference to it in the
 * table is reset to -1 first).  The chosen slot gets the name
 * (game_strcpy is strcpy) and age 0.  No arcade ancestor identified.
 * Also matches at -O2 and as a group with both real callees kept.
 *
 * What the match depends on:
 *  - `victim` is an unsigned copy of `best`: the unsigned compare makes uopt
 *    hoist the conversion as its own register (`move a1,t2`), and the second
 *    s32-sized local after `slot` gives the 120-byte frame.  `== best`
 *    directly, or an s32/s16 copy, puts `best` in a2 and gives a 112-byte
 *    frame (103 words differ).
 *  - Declaration order sets the homes: i/j/k above references[], then
 *    oldest, best, slot, victim.
 *  - `best` is read uninitialised when no age is above 0, as in retail
 *    (`lh t2,68(sp)` before the scan).
 */
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;

extern u8 gTempNames[20][13];
extern u16 gTempNameAges[20];
extern intptr_t gTotalNames[12][3][5];   /* a name slot or a profile handle (docs/port/CASTS.md) */
extern s32 game_strcmp(u8 *, u8 *);
extern u8 *game_strcpy(u8 *, u8 *);

s16 FindTempNameIdx(u8 *name)
{
    s16 i, j, k;
    s16 references[20];
    s16 oldest, best;
    intptr_t slot;
    u32 victim;

    for (i = 0; i < 20; i++) {
        gTempNameAges[i]++;
    }
    for (i = 0; i < 20; i++) {
        if (game_strcmp(gTempNames[i], name) == 0) {
            gTempNameAges[i] = 0;
            return i;
        }
    }
    for (i = 0; i < 20; i++) {
        if (gTempNames[i][0] == 0) {
            game_strcpy(gTempNames[i], name);
            gTempNameAges[i] = 0;
            return i;
        }
    }
    for (i = 0; i < 20; i++) {
        references[i] = 0;
    }
    for (j = 0; j < 12; j++) {
        for (k = 0; k < 3; k++) {
            for (i = 0; i < 5; i++) {
                slot = gTotalNames[j][k][i];
                if (slot >= 0 && slot < 20) {
                    references[slot]++;
                }
            }
        }
    }
    for (i = 0; i < 20; i++) {
        if (references[i] == 0) {
            game_strcpy(gTempNames[i], name);
            gTempNameAges[i] = 0;
            return i;
        }
    }
    oldest = 0;
    for (i = 0; i < 20; i++) {
        if (oldest < gTempNameAges[i]) {
            oldest = gTempNameAges[i];
            best = i;
        }
    }
    victim = best;
    for (j = 0; j < 12; j++) {
        for (k = 0; k < 3; k++) {
            for (i = 0; i < 5; i++) {
                if (gTotalNames[j][k][i] == victim) {
                    gTotalNames[j][k][i] = -1;
                }
            }
        }
    }
    game_strcpy(gTempNames[best], name);
    gTempNameAges[best] = 0;
    return best;
}

