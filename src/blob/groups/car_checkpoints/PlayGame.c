#include "game/game.h"

typedef struct Car952 {
    u8 pad000[237];
    s8 restart;            /* 0x0ED */
    u8 pad0EE[0x110 - 0xEE]; struct pc_visual visuals[21]; u8 pad0EE_308[0x35C - 0x308];
    s8 cam;                 /* 0x35C */
    s8 camMode;             /* 0x35D */
    s8 camModeSave;         /* 0x35E */
    u8 pad35F[0x380 - 0x35F]; void *input; u8 pad35F_384[0x3B8 - 0x384];
} Car952;

typedef struct Player76 { u8 pad00[64]; u16 numCrashes; u8 pad42[0x48 - 0x42]; void *ref; } Player76;
typedef struct Cam152 { u8 pad00[36]; f32 pos[3]; u8 pad30[84]; f32 savepos[3]; u8 pad90[8]; } Cam152;

typedef struct Inner5 { u8 pad0[5]; s8 f5; } Inner5;
/* A ghost (SaveInit' Ghost): link, file and data handles before its header, pointer-sized on the PC
 * (docs/port/CASTS.md). */
typedef struct Obj40 { void *ll; void **fileh; u8 pad8[0x24 - 8]; void *datah; Inner5 **inner; } Obj40;

extern Player76 gPlayers[];

VIEW(Cam152[], gCamera);
#define gCamera gCamera_view

VIEW(Obj40 **[], gGhosts);
#define gGhosts gGhosts_view
VIEW(u8[], unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view

void PlayGame(void) {
    s32 i;
    s16 slot;
    s32 cam;
    Car952 *car;

    Update_MDrive();
    if (gRace == 4) {
        CheckForStunt();
    } else if (gRace == 6) {
        ShotUpdate();
        UpdateBattlePowerUps();
    } else {
        if (gRace == 5) {
            GetResPt();
        } else {
            DoDrones();
        }
        CheckCPs();
    }
    UpdateSmoke();
    update_player_visibility();
    for (i = 0; i < gNumActiveCars; i++) {
        car = PC_CAR(i);
        slot = PC_MODEL_SLOT(i);
        if (*(volatile s8 *)&car->restart < 0) {
            cam = car->cam;
            if (!(gGameState & 8) && slot < gNumPlayers) {
                gPlayers[slot].numCrashes++;
            }
            if (cam >= 0) {
                car->camModeSave = car->camMode;
                car->camMode = 4;
                gCamera[cam].savepos[0] = gCamera[cam].pos[0];
                gCamera[cam].savepos[1] = gCamera[cam].pos[1];
                gCamera[cam].savepos[2] = gCamera[cam].pos[2];
            }
            StartBlast(&slot, 1, 1.f, 0);
            StopCarSounds(PC_MODEL(i));
            if (PC_MODEL_DRONE_TYPE(i) == 2) {
                if (gRace != 2 || gGhosts[PC_MODEL_SLOT(i)] == 0 || (*gGhosts[PC_MODEL_SLOT(i)])->inner[0]->f5 < 0) {
                    PlayGameSound(45, PC_MODEL_SLOT(i), 1, 1);
                }
            } else {
                EmitGameSound(PC_MODEL_RWR(i), (const f32 *)unitmatrix_origin, 400.0f, 0.0f, 1.0f, 0.0f, 45, PC_MODEL_SLOT(i), 0, 128);
            }
            if (gRace == 4) {
                CrashedStunt(slot);
            }
            car->restart = 0;
        } else if (*(volatile s8 *)&car->restart > 0) {
            InitSmokeBlast(slot);
            if (car->cam >= 0) {
                car->camMode = car->camModeSave;
                SetCameraView(car->cam);
            }
            car->restart = 0;
        }
        UpdateVisuals(slot);
        if ((PC_LINK_OWNER(slot) == 0 || PC_LINK_OWNER(slot) == 6) && (gRace != 2 || (gRace == 2 && slot == 0))) {
            CheckTargetCollision(slot);
        }
    }
    TextureAnimation();
    DetectDroneAbort();
    if (!gRestartGame) {
        ShowHUD();
    }
}
