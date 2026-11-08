/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * update_player_visibility (0x800AEB54, 452 B): historical label. N64-only player-pair visibility matrix
 * (no arcade counterpart found). Clears the 4x4 byte matrix gFadePlayerArrow, then, when the enable flag
 * gShowPlayerArrows is set and gRace is 6, casts a segment between every player i < count-1 and every
 * player j >= 1 (the inner loop restarts at 1 for each i, including j == i and repeated pairs), both
 * endpoints raised 5 units in y, through ShotToWorldCollision (historical label: a segment/collision
 * test taking start, end, a 9-float result, 0.5f, 0, 0xFFFF). A nonnull result marks [i][j] and [j][i].
 *
 * Source-form notes (no shaping devices, no unused locals):
 * - gFadePlayerArrow is s8 (lb/sb in all users): the stored 1 shares the s32 1 web, so j = 1 is `move s1,s3`.
 * - The y offset is `v[1] += 5.0f` after the three copies (retail adds in $f0, a coloured web).
 * - The clear loop uses its own index k declared after i, j (all three take frame slots: 12 B above
 *   first[]), written on three lines (one line changes as1's delay-slot fill of the unrolled stores).
 *
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;

typedef struct Player {        /* 952 B player/car record */
    u8 pad000[8];
    f32 position[3];           /* 0x08 */
    u8 pad014[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad014_308[0x380 - 0x308]; void *input; u8 pad014_384[0x3B8 - 0x384];
} Player;

extern s8 gFadePlayerArrow[4][4];    /* visibility matrix */
extern s8 gShowPlayerArrows;          /* enable flag */
extern s32 gRace;
extern s16 gNumPlayers;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

u16 *ShotToWorldCollision(void *arg0, void *arg1, f32 *arg2, f32 arg3, s32 arg4, s32 arg5);

void update_player_visibility(void) {
    s32 i, j, k;
    f32 first[3], second[3];
    f32 result[9];

    for (i = 0; i < 4; i++)
        for (k = 0; k < 4; k++)
            gFadePlayerArrow[i][k] = 0;
    if (gShowPlayerArrows && gRace == 6) {
        for (i = 0; i < gNumPlayers - 1; i++) {
            first[0] = PC_CAR_POS(i)[0];
            first[1] = PC_CAR_POS(i)[1];
            first[2] = PC_CAR_POS(i)[2];
            first[1] += 5.0f;
            for (j = 1; j < gNumPlayers; j++) {
                second[0] = PC_CAR_POS(j)[0];
                second[1] = PC_CAR_POS(j)[1];
                second[2] = PC_CAR_POS(j)[2];
                second[1] += 5.0f;
                if (ShotToWorldCollision(first, second, result, 0.5f, 0, 0xFFFF)) {
                    gFadePlayerArrow[i][j] = 1;
                    gFadePlayerArrow[j][i] = 1;
                }
            }
        }
    }
}
