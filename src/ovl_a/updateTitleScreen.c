/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Runtime A updateTitleScreen, [0x8038F454,0x8038F648), 500 bytes.
 * Strict standalone MATCH, 125/125 words, normal O3 plus r4300_mul.
 * Timed three-descriptor prompt, input/expiry branches and player-state reset.
 * Historical helper names identify their native addresses; original function
 * naming and exact arcade ancestry are not established.
 *
 * The native repeated handle=-1 writes and following handle!=-1 guard are
 * retained. IDO leaves a compare against identical registers for that redundant
 * guard, as in the target. This mirrors native behavior; it does not prove an
 * original source idiom or the identity of any formerly inlined helper.
 * The PlayerStatus view records only the observed state byte and 76-byte stride.
 * AudPlayEffect's real unsigned return/parameter contract is required for
 * the final loop's register allocation. No false return prototype, stand-in
 * callers, unused locals, assembly, volatility or private compiler flags.
 */
typedef signed char s8;
typedef unsigned int u32;
typedef int s32;
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
extern u32 gSkipToGameState, gGameState;
extern s32 movieIdx, titlescr_inited;
extern s8 gIntroMovieShown, gGameStarted, gSoftReset;
extern Blit *titlescr_blit;
extern MultiBlit titlescr_blits[];
extern u32 gActiveEdges;
extern PlayerStatus gPlayers[4];
extern void setGameState(s32);
extern Blit *NewMultiBlit(short, short, const MultiBlit *, short);
extern void RemoveBlit(Blit *);
extern void SetCountdownTimer(float);
extern s32 CountdownTimeOut(void);
extern void ClearRushControls(void);
extern u32 AudPlayEffect(u32, u32, u32, unsigned char);

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
