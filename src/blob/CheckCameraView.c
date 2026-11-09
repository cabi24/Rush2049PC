/* CheckCameraView: which view each window's camera shows, each frame (MAT3 camera.c). */
#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
/* The car record (MAT3 tCarData; the PC's pc_car, host layout): the fields this reads and sets. */
typedef struct Object {
    u8 other0[8];
    Vec3 pos;           /* 8 */
    u8 other20[60];
    Mat3 uvs;           /* 80 */
    u8 other116[128];
    s32 node;           /* 244 */
    u8 other248[0x110 - 0xF8]; struct pc_visual visuals[21]; u8 other248_308[0x35B - 0x308];
    s8 car;             /* 859 its slot */
    s8 slot;            /* 860 its camera (MAT3 camera), -1 none */
    s8 view;            /* 861 its camera's view */
    s8 view2;           /* 862 the view to go back to after the crash view (MAT3 saveview) */
    u8 other863[0x380 - 0x35F]; void *input; u8 other863_384[0x3B8 - 0x384];
} Object;               /* 952 */
/* The model record (MAT3 tModelData): the suspension and the fields this reads. */
typedef struct Model {
    u8 other0[1484];
    f32 sus[4];         /* 1484 */
    u8 other1500[16];
    f32 comp[4];        /* 1516 */
    u8 other1532[460];
    s16 active;         /* 1992 */
    u8 other1994[2];
    s8 f1996;           /* 1996 */
    u8 other1997[18];
    s8 f2015;           /* 2015 */
    u8 other2016[40];
} Model;                /* 0x808 */
/* A player's profile (SaveSlot, host layout: pc_prelude.h), and its handle. */
typedef struct { void *next, *prev, *fileh; u8 other0C[32]; void *resource; } HObject;
typedef struct Handle { HObject *object; } Handle;
/* A player's input record (gPlayers): the fields this reads. */
typedef struct {
    u8 index;
    u8 other1[3];
    s32 mask;           /* 4 */
    u8 other8[44];
    s32 edges;          /* 52 */
    u8 other56[16];
    Handle *handle;     /* 72 */
} Player76;

extern Player76 gPlayers[];


/* Picks each window's view and moves its camera (update_camera), then places the drones' scene nodes where their cars
 * are. In a ghost race the second controller's button 4 (with 2 held) moves window 0 to the next car in the race.
 * In the demo the view changes at random every 2.5 to 3.5 seconds (but the crash view, or near the countdown's end):
 * views 0 to 3, a view3 (10) or a track camera (6, for 3 seconds); a player's finish turns to the circling finish
 * view (5); a player's view button steps through views 0 to 3 (saved in their profile). The views: 0 and 1 the chase
 * views (1 lagging the car's acceleration, cur_acc), 2 and 3 view3's, 4 the crash, 5 the finish, 6 a track camera.
 * Also each window's car's suspension average for the in-car view (gTireCnt, gSusAvg: tyres compressed under 4). */
void CheckCameraView(void)
{
    s16 j;
    s32 k;
    s32 view;
    s32 slot;
    Object *obj;
    Model *m;
    tCamera *cam;
    s32 i;
    Player76 *pl;
    s32 n;
    s32 new_view3, block, delta, extra_lag; /* unused: see header (frame residual) */
    u32 edge;                               /* unused */
    f32 view3_lim;                          /* unused */

    if (gRace == kGhost && (gContLevels[1] & 2) && (gContEdges[1] & 4)) {
        n = gCamera[0].slot;
        PC_CAR_VIEW(n) = -1;
        do {
            n++;
            if (n >= gNumCars) {
                n = 0;
            }
        } while (PC_MODEL_ACTIVE(n) == 0 && PC_CAR_VIEW(n) < 0);
        PC_CAR_VIEW(n) = 0;
        gCamera[0].slot = n;
    }
    pl = gPlayers;
    for (i = 0; i < gNumWindows; i++, pl++) {
        cam = &gCamera[i];
        obj = PC_CAR(cam->slot);
        m = PC_MODEL(cam->slot);
        view = obj->view;
        slot = obj->slot;
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
        } else if (PC_FINISH_CAM(pl->index) != 0 && view != 4) {
            view = 5;
        } else if (view != 4 && (pl->edges & pl->mask)) {
            view++;
            if (view >= 4) view = 0;
            if (pl->handle->object->resource) {
                SavePlayerCarView(pl->handle, view);
            }
            if (view == 1) {
                cur_acc[obj->slot][0] = 0.0f;
                cur_acc[obj->slot][1] = 0.0f;
                cur_acc[obj->slot][2] = 0.0f;
            } else if (view == 2) {
                init_view3(obj);
                gCamera[obj->slot].fcam.pos[0] = obj->pos[0];
                gCamera[obj->slot].fcam.pos[1] = obj->pos[1] + 1.0f;
                gCamera[obj->slot].fcam.pos[2] = obj->pos[2];
            } else if (view == 3) {
                init_view3(obj);
            }
        }
        if (view != obj->view) {
            obj->view2 = view;
            obj->view = view;
        }
        update_camera(obj);
        gTireCnt[slot] = 0; gSusAvg[slot] = 0;
        for (k = 0; k < 4; k++) {
            if (m->comp[k] < 4.0f) {
                gTireCnt[slot]++;
                gSusAvg[slot] += m->sus[k];
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
        if (PC_MODEL_ACTIVE(j) != 0 && PC_CAR_NODE(j) >= 0 && PC_MODEL_DRONE_TYPE(j) != 2 && PC_MODEL_HIDE_CAR(j) == 0) {
            cam = ZOID_FindMatrix(PC_CAR_NODE(j));
            cam->mat.pos[0] = PC_CAR_POS(j)[0];
            cam->mat.pos[1] = PC_CAR_POS(j)[1];
            cam->mat.pos[2] = PC_CAR_POS(j)[2];
            cam->mat.pos[1] += 0.25f;
            CopyUV(PC_CAR_UVS50(j), cam);
        }
    }
}
