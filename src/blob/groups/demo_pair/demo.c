#include "game/game.h"

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

DemoBlit *NewMultiBlit(s16 arg0, s16 arg1, void *arg2, s16 arg3);
void RemoveBlit(DemoBlit *sound);

s32 CountdownTimeOut(void);

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
