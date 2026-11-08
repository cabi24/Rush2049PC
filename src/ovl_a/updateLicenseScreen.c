/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Runtime image A startup/input gate at 0x8039A2A0, complete 416-byte body.
 * Checks bit 0 for each of four controller words before initialization calls.
 * First-entry input takes an immediate state transition; otherwise it creates
 * a two-BLIT timed prompt and later releases it on deadline or gated input.
 * Historical helper names are retained as address identities, not semantics.
 * No exact arcade donor or original function name is claimed.
 *
 * Natural O3 four-element loop unrolling and declaration order reproduce the
 * target. Both locals are used; there are no dummy reads, unused locals,
 * invented formal arguments, forced volatility, assembly, or owned data.
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
extern s8 pfsInited;
extern s8 lcdInited;
extern s8 gBlocksNeeded;
extern s8 licensescr_inited;
extern s8 gLicenseShown;
extern u32 gContLevels[4];
extern u32 gActiveEdges;
extern Blit *licensescr_blit;
extern MultiBlit licensescr_blits[];
/* The seven-argument callback contract is evidenced by its actual dispatcher:
 * player/code, two unused incoming words, two signed-byte pointers and an
 * integer predicate. The nested predicate's argument contract is unresolved;
 * this function carries the callback address and invokes neither callback. */
typedef s32 (*StartupPredicate)();
typedef void (*StartupHandler)(s32, s32, s32, s32, s8 *, s8 *, StartupPredicate);
extern void PS_ErrorFunc(s32, s32, s32, s32, s8 *, s8 *, StartupPredicate);
extern void PS_InitPakSys(StartupHandler);
extern void PS_PakServer(s32, s32);
extern s32 PS_GetBlocks(s32);
extern void setGameState(s32);
extern Blit *NewMultiBlit(short, short, const MultiBlit *, short);
extern void RemoveBlit(Blit *);
extern void SetCountdownTimer(float);
extern s32 CountdownTimeOut(void);

void updateLicenseScreen(void)
{
    s8 pressed;
    s32 i;

    if (!pfsInited) {
        pressed = 0;
        for (i = 0; i < 4; i++) {
            pressed |= (gContLevels[i] & 1) != 0;
        }
        PS_InitPakSys(PS_ErrorFunc);
        PS_PakServer(1, 0);
        if (!lcdInited) {
            lcdInited = 1;
        }
        pfsInited = 1;
        if (pressed) {
            gBlocksNeeded = PS_GetBlocks(0x810);
            setGameState(0x04000000);
            return;
        }
    }
    if (!licensescr_inited) {
        licensescr_blit = NewMultiBlit(0, 0, licensescr_blits, 2);
        licensescr_inited = 1;
        SetCountdownTimer(3.0f);
    }
    if (CountdownTimeOut() || (gLicenseShown && (gActiveEdges & 7))) {
        if (licensescr_blit) {
            RemoveBlit(licensescr_blit);
            licensescr_blit = 0;
        }
        licensescr_inited = 0;
        setGameState(gLicenseShown ? 4 : 0x40000000);
        gLicenseShown = 1;
    }
}
