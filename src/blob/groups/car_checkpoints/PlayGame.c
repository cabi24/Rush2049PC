/* PlayGame: one frame of a race in progress (MAT3 game.c). */
#include "game/game.h"

/* The car record (MAT3 tCarData; the PC's pc_car, host layout; MAT3's offsets past the visuals are 0x18 more). */
typedef struct Car952 {
    u8 pad000[237];
    s8 restart;             /* 0x0ED set by the model: negative it crashed, positive it is back on the road */
    u8 pad0EE[0x110 - 0xEE]; struct pc_visual visuals[21]; u8 pad0EE_308[0x35C - 0x308];
    s8 camera;              /* 0x35C the camera showing it, -1 none */
    s8 view;                /* 0x35D that camera's view (4 the crash view) */
    s8 saveview;            /* 0x35E the view to go back to after the crash */
    u8 pad35F[0x380 - 0x35F]; void *input; u8 pad35F_384[0x3B8 - 0x384];
} Car952;

/* A player's input record (gPlayers): the field this counts. */
typedef struct Player76 { u8 pad00[64]; u16 numCrashes; u8 pad42[0x48 - 0x42]; void *ref; } Player76;

/* A ghost's save header (GhostSaveHeader): the flag that says it is a ghost to race against. */
typedef struct GhostHeaderView { u8 pad0[5]; s8 flag; } GhostHeaderView;
/* A ghost (SaveInit's Ghost): link, file and data handles before its header, pointer-sized on the PC
 * (docs/port/CASTS.md). */
typedef struct Obj40 { void *ll; void **fileh; u8 pad8[0x24 - 8]; void *datah; GhostHeaderView **header; } Obj40;

extern Player76 gPlayers[];


VIEW(Obj40 **[], gGhosts);
#define gGhosts gGhosts_view
/* unitmatrix's fourth row, the origin: the crash sound's orientation */
VIEW(u8[], unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view

/* Runs one frame of the race on the game thread: the cars' model results (Update_MDrive), the race's own rules
 * (stunts; battle's shots and power-ups; or the drones, or the obstacle course's respawn points, and the
 * checkpoints), smoke and the arrows. Then for each car: a crash (counted for a human player but in the attract
 * mode's demo) turns its camera to the crash view from where it was, blasts it, stops its sounds and plays the
 * crash, which a human hears direct (but in a ghost race against a ghost whose flag is set) and a drone's is
 * placed at the car; a car back on the road gets its smoke and its camera back. Its visuals, and its hits on the
 * track's targets for a car driven here (only the first player's in a ghost race). Then texture animation, drones
 * that want to give up, and the HUD. */
void PlayGame(void) {
    s32 i;
    s16 slot;
    s32 camera;
    Car952 *car;

    Update_MDrive();
    if (gRace == kStunt) {
        CheckForStunt();
    } else if (gRace == kBattle) {
        ShotUpdate();
        UpdateBattlePowerUps();
    } else {
        if (gRace == kObstacle) {
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
        /* volatile: the model thread sets restart; it is read again below, as MAT3 does */
        if (*(volatile s8 *)&car->restart < 0) {
            camera = car->camera;
            if (!(gGameState & GS_DEMO) && slot < gNumPlayers) {
                gPlayers[slot].numCrashes++;
            }
            if (camera >= 0) {
                car->saveview = car->view;
                car->view = 4;
                gCamera[camera].fcam.pos[0] = gCamera[camera].mat.pos[0];
                gCamera[camera].fcam.pos[1] = gCamera[camera].mat.pos[1];
                gCamera[camera].fcam.pos[2] = gCamera[camera].mat.pos[2];
            }
            StartBlast(&slot, 1, 1.f, 0);
            StopCarSounds(PC_MODEL(i));
            if (PC_MODEL_DRONE_TYPE(i) == 2) {
                if (gRace != kGhost || gGhosts[PC_MODEL_SLOT(i)] == 0 || (*gGhosts[PC_MODEL_SLOT(i)])->header[0]->flag < 0) {
                    PlayGameSound(45, PC_MODEL_SLOT(i), 1, 1); /* the crash (MAT3 9) */
                }
            } else {
                EmitGameSound(PC_MODEL_RWR(i), (const f32 *)unitmatrix_origin, 400.0f, 0.0f, 1.0f, 0.0f, 45, PC_MODEL_SLOT(i), 0, 128);
            }
            if (gRace == kStunt) {
                CrashedStunt(slot);
            }
            car->restart = 0;
        } else if (*(volatile s8 *)&car->restart > 0) {
            InitSmokeBlast(slot);
            if (car->camera >= 0) {
                car->view = car->saveview;
                SetCameraView(car->camera);
            }
            car->restart = 0;
        }
        UpdateVisuals(slot);
        if ((PC_LINK_OWNER(slot) == 0 || PC_LINK_OWNER(slot) == 6) && (gRace != kGhost || (gRace == kGhost && slot == 0))) {
            CheckTargetCollision(slot);
        }
    }
    TextureAnimation();
    DetectDroneAbort();
    if (!gRestartGame) {
        ShowHUD();
    }
}
