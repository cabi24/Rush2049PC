typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct {            /* the profile's host layout (pc_prelude.h): unk08 is its pak file handle */
    void *next, *prev;
    void *unk08;
    u8 pad0C[8];
    char name[1];
} PlayerProfile;

typedef struct {
    u8 pad00;
    u8 active;
    u8 pad02[0x48 - 2];
    PlayerProfile **profile;
} InputRecord;

extern s16 gNumPlayers;
extern InputRecord gPlayers[];
/* Each player's name slot (an index into gTempNames, -1 for none) or profile handle (&gJustPlayers[n], host storage),
 * pointer-sized on the PC like the high-score table gTotalNames it is copied into (docs/port/CASTS.md). */
extern intptr_t gThisPlayer[];
#define NO_NAME (-1)
extern s8 gDrawName[];
extern char gScoreName[][13];
extern s16 highscore_gNameSelect[];
extern s16 highscore_gNameLen[];
extern s8 gNewTotalScore[3][5];
extern char gTempNames[][13];
extern s32 gLastPlayer[];

void *memset(void *dst, s32 c, u32 n);
u32 strlen(const char *s);
void game_strcpy(char *dst, char *src);

void RankScores(void)
{
    s32 i;
    s32 j;
    s32 k;
    PlayerProfile **profile;

    for (i = 0; i < gNumPlayers; i++) {
        profile = gPlayers[i].profile;
        if ((*profile)->unk08 == 0) {
            gThisPlayer[i] = gLastPlayer[gPlayers[i].active];
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
        if ((*profile)->unk08 == 0) {
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

