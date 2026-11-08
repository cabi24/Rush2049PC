/* Native demo/attract research (refs/pr/413; lever). updateDemoGame,
 * playDemoGame = playDemoGame (MAT3 demo.c).
 * Shaping (disclosed, compile-affecting): playDemoGame has a SECOND formal `x` (the GC source calls it with
 * two actual arguments, DWARF lists one formal; every caller passes 0) and a compiled-out holder `if (x) {}`
 * before the PrePlay test. x is then a code-free coloured web live across calls, which IPA places in s0, giving
 * retail's `move s0,zero` at the three call sites (without it updateDemoGame is 3/133 words off).
 * Native stub identities are inferred from the exact adjacent code layout and the genuine donor call
 * boundaries; their eight-byte bodies are the retail deleted-procedure stubs.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    u8 pad0[0x7C6];
    s16 slot;
    u8 pad7C8[0x808 - 0x7C8];
} MODELDAT;

typedef struct {
    u8 node;
    u8 pad1[0x48 - 0x1]; void *ref;
} Player;

static s8 demo_preplayDone = 0;
extern s16 gNumWindows;
extern u32 gGameState;
extern u32 gNewGameState;
extern s32 gSkipToGameState;
extern s32 gRace;
static s16 gCurrentDemoTrack = -1;
extern s8 gNumActiveCars;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
typedef struct DemoVisual DemoVisual;
extern DemoVisual *gEnvList;
extern Player gPlayers[4];
extern s8 gTrackNum;
static s8 demo_inited = 0;
typedef struct DemoBlit DemoBlit;
static DemoBlit *demo_blit = 0;
extern struct pc_multiblit demo_multiblit[1];   /* the demo's blit, NewMultiBlit's descriptor (pc_prelude.h) */
extern s32 gShowCredits;
extern s8 expansion_pak_present;
extern s8 gSoftReset;
extern s32 gActiveEdges;

void fake_init(s16 arg0);
s32 PrePlay(s32 arg0, s32 arg1);
void CarSetup(void);
void Update_MDrive(void);
void InitCPTime(void);
void PlayGame(void);
void End_MDrive(s16 index);
void modelstop(void);
void StopListeners(void);
void RemoveHUD(void);
void ReleaseEnviron(DemoVisual *node, s32 arg1);
void InitSkids(void);
void RemoveCar(s16 arg0);
void UnloadTrack(s32 arg0);
DemoBlit *NewMultiBlit(s16 arg0, s16 arg1, void *arg2, s16 arg3);
void RemoveBlit(DemoBlit *sound);
void SetCountdownTimer(f32 arg0);
s32 CountdownTimeOut(void);
void ClearRushControls(void);

/* The game's own setGameState and RemoveEnvirons (this file had static copies: the same code). */
void setGameState(u32 state);
void playDemoGame(s32 on, s32 x);
void initDemoGame(void);
void uninitDemoGame(void);
void nextAvailDemoTrack(void);
void endDemoGame(void);
void RemoveEnvirons(void);

void updateDemoGame(void) {
    s32 x;

    if (!demo_inited) {
        initDemoGame();
    }
    playDemoGame(1, 0);
    if (gSoftReset && demo_preplayDone) {
        uninitDemoGame();
        setGameState(4);
        return;
    }
    if ((gActiveEdges & 7) && demo_preplayDone || CountdownTimeOut()) {
        x = (gActiveEdges & 1) != 0;
        uninitDemoGame();
        if (gCurrentDemoTrack == 5 && CountdownTimeOut()) {
            setGameState(2);
            return;
        }
        setGameState(4);
        if (x) {
            gSkipToGameState = 16;
        }
    }
}

void initDemoGame(void) {

        demo_blit = NewMultiBlit(0, 0, &demo_multiblit, 1);
        SetCountdownTimer(30.0f);
        nextAvailDemoTrack();
        demo_inited = 1;
        ClearRushControls();
    }

void uninitDemoGame(void) {
        if (demo_blit) {
            RemoveBlit(demo_blit);
            demo_blit = 0;
        }
        playDemoGame(0, 0);
        demo_inited = 0;
}

void playDemoGame(s32 on, s32 x) {
    static s8 gDemoInited = 0;

    if (on) {
        if (!gDemoInited) {
            gDemoInited = 1;
            gNumWindows = 1;
            if (!(gGameState & 0x02000000)) {
                gRace = 0;
                fake_init(gCurrentDemoTrack);
            }
        }
        if (x) {}
        if (!demo_preplayDone && PrePlay(gGameState & 8, 0)) {
            CarSetup();
            Update_MDrive();
            InitCPTime();
            demo_preplayDone = 1;
        } else if (demo_preplayDone) {
            PlayGame();
        }
    } else {
        gDemoInited = 0;
        demo_preplayDone = 0;
        endDemoGame();
        if (!(gGameState & 0x02000000)) {
            UnloadTrack(gTrackNum);
        }
    }
}

void endDemoGame(void) {
    s16 i;
        for (i = 0; i < gNumActiveCars; i++) {
            End_MDrive(PC_MODEL_SLOT(i));
        }
        modelstop();
        StopListeners();
        RemoveHUD();
        RemoveEnvirons();
        for (i = 0; i < gNumActiveCars; i++) {
            RemoveCar(i);
        }
        for (i = 0; i < 4; i++) {
            gPlayers[i].node = i;
        }
}

void nextAvailDemoTrack(void) {
        gCurrentDemoTrack++;
        gShowCredits = 0;
        if (gCurrentDemoTrack >= 6) {
            gShowCredits = 1;
            gCurrentDemoTrack = 0;
        }
        if (!expansion_pak_present && gCurrentDemoTrack >= 5) {
            gShowCredits = 1;
            gCurrentDemoTrack = 0;
        }
}
