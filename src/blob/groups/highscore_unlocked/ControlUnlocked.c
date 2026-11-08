typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;

typedef struct {
    u8 pad00[4];
    s32 unk04;
    u8 pad08[0x48 - 0x8]; void *ref;
} InputRecord;

extern s16 gNumPlayers;
extern s32 gRace;
extern InputRecord gPlayers[];
extern s8 gStuffUnlocked[];
extern s8 circuitCode;
extern s8 *namePanes[];
extern s32 gHighScoreState;
extern intptr_t highscore_blit;

void RemovePane(s8 *arg0);
s32 CountdownTimeOut(s32 arg0);
void PlayStartABSound(s32 arg0);
void RemoveBlit(s32 arg0);

extern s8 *codePane;
extern s8 *ghostPanes[3];

void SetHighScoreState(s32 mode);

void ControlUnlocked(void)
{
    s32 i;
    s32 done;

    done = 1;
    for (i = 0; i < gNumPlayers; i++) {
        if (gStuffUnlocked[i] == 1) {
            if ((gPlayers[i].unk04 & 3) || CountdownTimeOut(gPlayers[i].unk04)) {
                PlayStartABSound(gPlayers[i].unk04);
                gStuffUnlocked[i] = 0;
            }
        } else if (namePanes[i] != 0) {
            RemovePane(namePanes[i]);
            namePanes[i] = 0;
        }
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (gStuffUnlocked[i] == 1) {
            done = 0;
        }
    }
    if (done == 1) {
        if (circuitCode) {
            SetHighScoreState(2);
        } else if (gRace == 0 || gRace == 3 || gRace == 2) {
            SetHighScoreState(3);
        } else {
            SetHighScoreState(0);
        }
    }
}

