#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit {
    const char *texname;
    short dulx, duly, width, height, top, bot, left, right;
    u32 zdepth, alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;

typedef struct PlayerStatus {
    unsigned char header, state, rest[0x48 - 2]; void *ref;
} PlayerStatus;

VIEW(MultiBlit[], titlescr_blits);
#define titlescr_blits titlescr_blits_view
VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view
extern PlayerStatus gPlayers[4];

extern Blit *NewMultiBlit(short, short, const MultiBlit *, short);
extern void RemoveBlit(Blit *);

extern s32 CountdownTimeOut(void);

void updateTitleScreen(void)
{
    s32 i;
    if (gSkipToGameState != 1) {
        if (gSkipToGameState != gGameState) {
            setGameState(0x10);
            return;
        }
        gSkipToGameState = 1;
    }
    if (movieIdx != -1)
        movieIdx = -1;
    if (movieIdx == -1 && gIntroMovieShown < 2) {
        movieIdx = -1;
        gIntroMovieShown++;
        if (movieIdx != -1)
            return;
    }
    if (!titlescr_inited) {
        titlescr_blit = NewMultiBlit(0, 0, titlescr_blits, 3);
        SetCountdownTimer(5.0f);
        titlescr_inited = 1;
        if (gGameStarted && gSoftReset) {
            gGameStarted = 0;
            gSoftReset = 0;
        }
        ClearRushControls();
    }
    if (gActiveEdges & 1) {
        if (movieIdx != -1)
            movieIdx = -1;
        if (titlescr_blit) {
            RemoveBlit(titlescr_blit);
            titlescr_blit = 0;
        }
        titlescr_inited = 0;
        AudPlayEffect(0x4F, 0, 1, 0);
        gPlayers[0].state = 0;
        for (i = 1; i < 4; i++)
            gPlayers[i].state = 5;
        setGameState(0x10);
    } else if (CountdownTimeOut()) {
        if (titlescr_blit) {
            RemoveBlit(titlescr_blit);
            titlescr_blit = 0;
        }
        titlescr_inited = 0;
        setGameState(8);
    }
}
