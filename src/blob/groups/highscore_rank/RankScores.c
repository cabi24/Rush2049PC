/* RankScores: who enters a name on the high-score screen (MAT3 highscorescr.c RankScores). */
#include "game/game.h"

/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS): fileh is its pak file handle, NULL
 * for a guest. */
typedef struct {
    void *next, *prev;
    void *fileh;
    u8 pad0C[8];
    char name[1];
} PlayerProfile;

/* A player's pad record (MAT3 tPadState, gPlayers). */
typedef struct InputRecord {
    u8 pad00;
    u8 controller;          /* 0x01 */
    u8 pad02[0x48 - 2];
    PlayerProfile **profile; /* 0x48 */
} InputRecord;

extern InputRecord gPlayers[];

#define NO_NAME (-1)                /* gThisPlayer[]: a guest without a name yet */

VIEW(s8[3][5], gNewTotalScore);
#define gNewTotalScore gNewTotalScore_view

void *memset(void *dst, s32 c, u32 n);
u32 strlen(const char *s);

/* For each player: the record holder it is (gThisPlayer: its profile, or a guest's last temporary name), and
 * whether it enters a name (gDrawName): a player whose new score made one of the three tables does, if it is a
 * guest, starting from its last name (or none); a profile's name is filled in without asking. */
void RankScores(void)
{
    s32 i;
    s32 j;
    s32 k;
    PlayerProfile **profile;

    for (i = 0; i < gNumPlayers; i++) {
        profile = gPlayers[i].profile;
        if ((*profile)->fileh == 0) {
            gThisPlayer[i] = gLastPlayer[gPlayers[i].controller];
        } else {
            gThisPlayer[i] = (intptr_t)profile;
        }
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 5; k++) {
                if (gNewTotalScore[j][k] == i) {
                    gDrawName[i] = 1;
                }
            }
        }
        if (gDrawName[i] == 0) {
            continue;
        }
        if ((*profile)->fileh == 0) {
            gDrawName[i] = 1;
            highscore_gNameSelect[i] = -1;
            if (gThisPlayer[i] == NO_NAME) {
                memset(gScoreName[i], 0, 13);
            } else {
                game_strcpy(gScoreName[i], gTempNames[gThisPlayer[i]]);
            }
            highscore_gNameLen[i] = strlen(gScoreName[i]);
        } else {
            gDrawName[i] = 0;
            game_strcpy(gScoreName[i], (*profile)->name);
        }
    }
}
