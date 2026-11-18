/* CheckCameraView: which view each window's camera shows, each frame (MAT3 camera.c). */
#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];

/* Picks each window's view and moves its camera (update_camera), then places the drones' scene nodes where their cars
 * are. In a ghost race the second controller's button 4 (with 2 held) moves window 0 to the next car in the race.
 * In the demo the view changes at random every 2.5 to 3.5 seconds (but the crash view, or near the countdown's end):
 * views 0 to 3, a view3 (10) or a track camera (6, for 3 seconds); a player's finish turns to the circling finish
 * view (5); a player's view button steps through views 0 to 3 (saved in their profile). The views: 0 and 1 the chase
 * views (1 lagging the car's acceleration, cur_acc), 2 and 3 view3's, 4 the crash, 5 the finish, 6 a track camera.
 * Also each window's car's suspension average for the in-car view (gTireCnt, gSusAvg: the tyres less
 * than 4 feet above the road). */
void CheckCameraView(void)
{
    s16 j;
    s32 k;
    s32 view;
    s32 slot;
    tCarData *obj;
    tModelData *m;
    tCamera *cam;
    s32 i;
    tPadState *pl;
    s32 n;
    s32 new_view3, block, delta, extra_lag; /* unused: see header (frame residual) */
    u32 edge;                               /* unused */
    f32 view3_lim;                          /* unused */

    if (gRace == kGhost && (gContLevels[1] & 2) && (gContEdges[1] & 4)) {
        n = gCamera[0].slot;
        pc_cars[n].camera = -1;
        do {
            n++;
            if (n >= gNumCars) {
                n = 0;
            }
        } while (pc_models[n].in_game == 0 && pc_cars[n].camera < 0);
        pc_cars[n].camera = 0;
        gCamera[0].slot = n;
    }
    pl = gPlayers;
    for (i = 0; i < gNumWindows; i++, pl++) {
        cam = &gCamera[i];
        obj = &pc_cars[cam->slot];
        m = &pc_models[cam->slot];
        view = obj->view;
        slot = obj->camera;
        if (gGameState & GS_DEMO) {
            if ((gModelTime - lastSwitch[slot] > camera_nextSwitch[slot] || gModelTime < lastSwitch[slot]) && view != 4 && GetCountdownTimeRemaining() > 3.0f) {
                lastSwitch[slot] = gModelTime;
                camera_nextSwitch[slot] = Random(1.0f) + 2.5;
                switch ((u32)Random(11.0f)) {
                case 0:
                    view = 0;
                    break;
                case 1:
                    view = 1;
                    cur_acc[slot][0] = 0.0f;
                    cur_acc[slot][1] = 0.0f;
                    cur_acc[slot][2] = 0.0f;
                    break;
                case 2:
                    if (view != 3) view = 2;
                    break;
                case 3:
                    if (view != 2) view = 3;
                    break;
                case 4:
                    init_view3(obj);
                    view = 10;
                    break;
                default:
                    view = 6;
                    init_camera_on_track(obj);
                    camera_nextSwitch[slot] = 3.0f;
                    break;
                }
            }
        } else if (PC_FINISH_CAM(pl->slot) != 0 && view != 4) {
            view = 5;
        } else if (view != 4 && (pl->controls[CONTROL_VIEW] & pl->edges)) {
            view++;
            if (view >= 4) view = 0;
            if ((*pl->save)->datah) {
                SavePlayerCarView(pl->save, view);
            }
            if (view == 1) {
                cur_acc[obj->camera][0] = 0.0f;
                cur_acc[obj->camera][1] = 0.0f;
                cur_acc[obj->camera][2] = 0.0f;
            } else if (view == 2) {
                init_view3(obj);
                gCamera[obj->camera].fcam.pos[0] = obj->dr_pos[0];
                gCamera[obj->camera].fcam.pos[1] = obj->dr_pos[1] + 1.0f;
                gCamera[obj->camera].fcam.pos[2] = obj->dr_pos[2];
            } else if (view == 3) {
                init_view3(obj);
            }
        }
        if (view != obj->view) {
            obj->saveview = view;
            obj->view = view;
        }
        update_camera(obj);
        gTireCnt[slot] = 0; gSusAvg[slot] = 0;
        for (k = 0; k < 4; k++) {
            if (m->airdist[k] < 4.0f) {
                gTireCnt[slot]++;
                gSusAvg[slot] += m->suscomp[k];
            }
        }
        if (gTireCnt[slot] >= 3) {
            gSusAvg[slot] /= (f32)gTireCnt[slot];
        } else {
            gSusAvg[slot] = 0;
        }
    }
    BlinkResurrectingCars();
    for (j = 0; j < PC_RACE_CARS; j++) {
        if (pc_models[j].in_game != 0 && pc_cars[j].objnum >= 0 && pc_models[j].drone_type != 2 && pc_models[j].hide_car == 0) {
            cam = ZOID_FindMatrix(pc_cars[j].objnum);
            cam->mat.pos[0] = pc_cars[j].dr_pos[0];
            cam->mat.pos[1] = pc_cars[j].dr_pos[1];
            cam->mat.pos[2] = pc_cars[j].dr_pos[2];
            cam->mat.pos[1] += 0.25f;
            CopyUV(pc_cars[j].dr_car_uvs, cam);
        }
    }
}
