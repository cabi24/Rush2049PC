/* updateTitleScreen: the title screen (MAT3 titlescr.c updateTitleScreen), GS_TITLE: Start goes on to the start
 * menu, five seconds without it start the attract mode (GS_DEMO). After a race it passes the player straight on
 * (gSkipToGameState). */
#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit {
    const char *texname;
    short dulx, duly, width, height, top, bot, left, right;
    u32 zdepth, alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;

/* A player's pad record (MAT3 tPadState, gPlayers). */
typedef struct PlayerStatus {
    unsigned char slot, controller, rest[0x48 - 2]; void *ref;
} PlayerStatus;

VIEW(MultiBlit[], titlescr_blits);
#define titlescr_blits titlescr_blits_view
VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view
extern PlayerStatus gPlayers[4];

extern s32 CountdownTimeOut(void);

#define SND_START 0x4F              /* the effect Start plays here */
#define CONTROLLER_NONE 5           /* gPlayers[].controller of a player without one */

/* The screen's frame. The first frame puts up its blits and starts a 5-second timer; a soft reset is over once the
 * title is back. Start: player 1 takes controller 1 (the others none, until the player screen) and the start
 * menu opens; the time running out starts the attract mode. */
void updateTitleScreen(void)
{
    s32 i;
    if (gSkipToGameState != SKIP_NONE) {
        if (gSkipToGameState != gGameState) {
            setGameState(GS_START);
            return;
        }
        gSkipToGameState = SKIP_NONE;
    }
    /* What is left of the intro movie (MAT3 plays one): no movie, so this only counts gIntroMovieShown up to 2. */
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
    if (gActiveEdges & BUTTON_START) {
        if (movieIdx != -1)
            movieIdx = -1;
        if (titlescr_blit) {
            RemoveBlit(titlescr_blit);
            titlescr_blit = 0;
        }
        titlescr_inited = 0;
        AudPlayEffect(SND_START, 0, 1, 0);
        gPlayers[0].controller = 0;
        for (i = 1; i < 4; i++)
            gPlayers[i].controller = CONTROLLER_NONE;
        setGameState(GS_START);
    } else if (CountdownTimeOut()) {
        if (titlescr_blit) {
            RemoveBlit(titlescr_blit);
            titlescr_blit = 0;
        }
        titlescr_inited = 0;
        setGameState(GS_DEMO);
    }
}
