/* The attract mode's demo race (MAT3 demo.c): every 30 seconds the title gives way to a drones' race on the next
 * track in turn, with the credits after the last. */
#include "game/game.h"

/* A player's input record (gPlayers): its node. */
typedef struct Player {
    u8 node;
    u8 pad1[0x48 - 0x1]; void *ref;
} Player;

static s8 demo_preplayDone = 0;

VIEW(s32, gSkipToGameState);
#define gSkipToGameState gSkipToGameState_view

static s16 gCurrentDemoTrack = -1;

extern Player gPlayers[4];

static s8 demo_inited = 0;
typedef struct DemoBlit DemoBlit;
static DemoBlit *demo_blit = 0;
VIEW(struct pc_multiblit[1], demo_multiblit);
#define demo_multiblit demo_multiblit_view

DemoBlit *NewMultiBlit(s16 x, s16 y, void *def, s16 count);
void RemoveBlit(DemoBlit *sound);

s32 CountdownTimeOut(void);

/* One frame of the demo (state GS_DEMO): set it up the first time, run it (playDemoGame); a soft reset or a button
 * (A, B or Start) once it runs, or its 30 seconds out, ends it back to the title (state 4: the start button takes
 * it on to state 16, and after the last track's demo the credits, state 2). */
void updateDemoGame(void) {
    s32 x;

    if (!demo_inited) {
        initDemoGame();
    }
    playDemoGame(1, 0);
    if (gSoftReset && demo_preplayDone) {
        uninitDemoGame();
        setGameState(GS_TITLE);
        return;
    }
    if ((gActiveEdges & 7) && demo_preplayDone || CountdownTimeOut()) {
        x = (gActiveEdges & 1) != 0;
        uninitDemoGame();
        if (gCurrentDemoTrack == 5 && CountdownTimeOut()) {
            setGameState(GS_LICENSE);
            return;
        }
        setGameState(GS_TITLE);
        if (x) {
            gSkipToGameState = GS_START;
        }
    }
}

/* Sets the demo up: its "demo" blit, its 30 seconds, the next track. */
void initDemoGame(void) {

        demo_blit = NewMultiBlit(0, 0, &demo_multiblit, 1);
        SetCountdownTimer(30.0f);
        nextAvailDemoTrack();
        demo_inited = 1;
        ClearRushControls();
    }

/* Ends the demo: its blit, its race. */
void uninitDemoGame(void) {
        if (demo_blit) {
            RemoveBlit(demo_blit);
            demo_blit = 0;
        }
        playDemoGame(0, 0);
        demo_inited = 0;
}

/* With `on`, runs the demo race: the first time a one-window single race on the demo track (fake_init; but in the
 * high score state), loaded and placed (PrePlay) before it plays (PlayGame). Without it, ends the race and unloads
 * its track. `x` is not read. */
void playDemoGame(s32 on, s32 x) {
    static s8 gDemoInited = 0;

    if (on) {
        if (!gDemoInited) {
            gDemoInited = 1;
            gNumWindows = 1;
            if (!(gGameState & GS_HIGH_SCORE)) {
                gRace = kOneRace;
                fake_init(gCurrentDemoTrack);
            }
        }
        if (x) {}
        if (!demo_preplayDone && PrePlay(gGameState & GS_DEMO, 0)) {
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
        if (!(gGameState & GS_HIGH_SCORE)) {
            UnloadTrack(gTrackNum);
        }
    }
}

/* The demo race's end: the cars' models and the model thread stop, the HUD, environment and cars go. */
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

/* The next demo track: 0 to 5 in turn (0 to 4 without the Expansion Pak), the credits shown after the last. */
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
