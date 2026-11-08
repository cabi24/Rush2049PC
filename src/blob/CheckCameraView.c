/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * CheckCameraView (0x800EB90C, 2,000 B): per-frame camera view selection for every viewport = arcade
 * game/camera.c CheckCameraView() (+ the suspension average and the drone-camera placement of setcamview()).
 *  - In gameplay mode 2 with gContLevels_pad2 bit 1 and gContEdges_pad2 bit 2: bump viewport 0's target car to the next
 *    active car (arcade "bump target drone"; car record gGameCar[] 952 B, model D_8014A250[] 0x808 B).
 *  - Per viewport i < gNumWindows (camera gCamera[i], its target car obj/model, player record pl):
 *    demo/attract (gGameState & 8): every camera_nextSwitch[slot] seconds (lastSwitch = last switch time,
 *    gModelTime = race clock) and if GetCountdownTimeRemaining() > 3, pick a random view (arcade
 *    `switch ((long)Random(11.0))`, here (u32), 0..4 + default: fixed-on-track via init_camera_on_track
 *    = respawn-point picker, lag 3 s); otherwise D_80110680[player] forces view 5, else the view button
 *    (pl->edges & pl->mask) cycles views 0..3 (SavePlayerCarView notifies the player's object, view 1 clears
 *    cur_acc cur_acc, view 2/3 re-init View3Cam via init_view3). Then update_camera (setcamview's
 *    switch) and the tire-suspension average (gTireCnt count / gSusAvg avg, gSusAvg).
 *  - BlinkResurrectingCars(), then place the drone cameras (ObjInstanceList[node].cam) at each active car + 0.25 up.
 * rand/Random are verbatim copies of the locked kept game_rand/Random (inlined in the unit, as in
 * pick_random_masked_bit); node_camera is an inlined static (its s16 parameter is retail's a0 narrowing).
 * Shaping devices (disclosed):
 *  - SIX UNUSED LOCALS (new_view3, block, delta, extra_lag, edge, view3_lim -- the names of arcade
 *    CheckCameraView's locals, which the N64 port dropped the uses of): exact frame residual. Retail frame
 *    200; without them 176 (`v/p7.c`, otherwise code-identical apart from the i home slot); 6 slots
 *    (5 gives 192). The 7 used locals declared before i put i's home at 168(sp) as in retail.
 *  - `extern volatile s16 gNumCars` (car count): retail reloads it every iteration of the bump loop
 *    through a hoisted address (lh t4,0(a0)); non-volatile hoists the load (459 words).
 * Other natural levers: index-form bump loop (`gGameCar[n].slot`, strength-reduced as in retail), both
 * suspension resets on one line and `pl++` in the for header (as1 line tie-breaks), `= 0` (not 0.0f) for
 * gSusAvg (separate zero web from the $f22 0.0f), `Random(1.0f) + 2.5` (double add as in retail).
 * Own .rodata: jump table 0x8012455C (5 entries).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
typedef struct {
    u8 other0[8];
    Vec3 pos;           /* 8 */
    u8 other20[60];
    Mat3 uvs;           /* 80 */
    u8 other116[128];
    s32 node;           /* 244 */
    u8 other248[0x110 - 0xF8]; struct pc_visual visuals[21]; u8 other248_308[0x35B - 0x308];
    s8 car;             /* 859 */
    s8 slot;            /* 860 */
    s8 view;            /* 861 */
    s8 view2;           /* 862 */
    u8 other863[0x380 - 0x35F]; void *input; u8 other863_384[0x3B8 - 0x384];
} Object;               /* 952 */
typedef struct {
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
typedef struct {
    Mat3 uvs;           /* 0 */
    Vec3 pos;           /* 36 */
    u8 other48[48];
    Mat3 out_uvs;       /* 96 */
    Vec3 out_pos;       /* 132 */
    f32 spring;         /* 144 */
    u8 target;          /* 148 */
    u8 other149[3];
} Camera;               /* 152 */
typedef struct { void *next, *prev, *fileh; u8 other0C[32]; void *resource; } HObject;   /* the profile, pc_prelude.h */
typedef struct { HObject *object; } Handle;
typedef struct {
    u8 index;
    u8 other1[3];
    s32 mask;           /* 4 */
    u8 other8[44];
    s32 edges;          /* 52 */
    u8 other56[16];
    Handle *handle;     /* 72 */
} Player76;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; Camera *cam; f32 scale, w; unsigned short model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Node68;
extern s32 gRace;
extern s32 gContLevels_pad2, gContEdges_pad2;
extern s32 gGameState;
extern volatile s16 gNumCars;
extern s16 gNumWindows;
extern f32 gModelTime;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Camera gCamera[];
extern Player76 gPlayers[];
extern Vec3 cur_acc[];
extern f32 lastSwitch[], camera_nextSwitch[];
/* On the PC the finish cameras are PC data, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 7), and
 * the drone cameras are placed for every car the race holds (docs/port/MORECARS.md 3.1). */
#define RACE_CARS PC_RACE_CARS
extern s8 gTireCnt[];
extern f32 gSusAvg[];
extern Node68 ObjInstanceList[];
extern s32 rand_seed;
extern f32 GetCountdownTimeRemaining(void);
extern void SavePlayerCarView(Handle *handle, u8 view);
extern void init_view3(Object *object);
extern void init_camera_on_track(Object *object);
extern void update_camera(Object *object);
extern void BlinkResurrectingCars(void);
extern void CopyUV(void *src, void *dst);

int game_rand(void);

f32 Random(f32 max);

Camera *ZOID_FindMatrix(s16 node);

void CheckCameraView(void)
{
    s16 j;
    s32 k;
    s32 view;
    s32 slot;
    Object *obj;
    Model *m;
    Camera *cam;
    s32 i;
    Player76 *pl;
    s32 n;
    s32 new_view3, block, delta, extra_lag; /* unused: see header (frame residual) */
    u32 edge;                               /* unused */
    f32 view3_lim;                          /* unused */

    if (gRace == 2 && (gContLevels_pad2 & 2) && (gContEdges_pad2 & 4)) {
        n = gCamera[0].target;
        PC_CAR_VIEW(n) = -1;
        do {
            n++;
            if (n >= gNumCars) {
                n = 0;
            }
        } while (PC_MODEL_ACTIVE(n) == 0 && PC_CAR_VIEW(n) < 0);
        PC_CAR_VIEW(n) = 0;
        gCamera[0].target = n;
    }
    pl = gPlayers;
    for (i = 0; i < gNumWindows; i++, pl++) {
        cam = &gCamera[i];
        obj = PC_CAR(cam->target);
        m = PC_MODEL(cam->target);
        view = obj->view;
        slot = obj->slot;
        if (gGameState & 8) {
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
                gCamera[obj->slot].out_pos[0] = obj->pos[0];
                gCamera[obj->slot].out_pos[1] = obj->pos[1] + 1.0f;
                gCamera[obj->slot].out_pos[2] = obj->pos[2];
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
    for (j = 0; j < RACE_CARS; j++) {
        if (PC_MODEL_ACTIVE(j) != 0 && PC_CAR_NODE(j) >= 0 && PC_MODEL_DRONE_TYPE(j) != 2 && PC_MODEL_HIDE_CAR(j) == 0) {
            cam = ZOID_FindMatrix(PC_CAR_NODE(j));
            cam->pos[0] = PC_CAR_POS(j)[0];
            cam->pos[1] = PC_CAR_POS(j)[1];
            cam->pos[2] = PC_CAR_POS(j)[2];
            cam->pos[1] += 0.25f;
            CopyUV(PC_CAR_UVS50(j), cam);
        }
    }
}
