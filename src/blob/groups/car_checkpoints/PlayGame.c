/* PlayGame: one frame of a race in progress (MAT3 game.c). */
#include "game/game.h"

/* unitmatrix's fourth row, the origin: the crash sound's orientation */

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
    tCarData *car;

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
    UpdateArrowVis();
    for (i = 0; i < gNumActiveCars; i++) {
        car = &pc_cars[i];
        slot = pc_models[i].slot;
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
            StopCarSounds(&pc_models[i]);
            if (pc_models[i].drone_type == 2) {
                if (gRace != kGhost || gGhosts[pc_models[i].slot] == 0 || (*gGhosts[pc_models[i].slot])->header[0]->flag < 0) {
                    PlayGameSound(45, pc_models[i].slot, 1, 1); /* the crash (MAT3 9) */
                }
            } else {
                EmitGameSound(pc_models[i].rwr, (const f32 *)unitmatrix_origin, 400.0f, 0.0f, 1.0f, 0.0f, 45, pc_models[i].slot, 0, 128);
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
