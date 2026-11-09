/* anim_tire_donor.c (visuals): a car's tire each frame (MAT3 visuals.c, AnimateTire()). */
#include "game/game.h"

typedef f32 Vec3[3];

/* A car visual (tVisual) on the PC: 0x20 bytes, next and func host pointers; a tire's data holds its turned angle
 * (docs/port/CARS.md 3). */
typedef struct tVisual {
    void *next;
    s16 index,objnum,slot,pad;   /* index: the tire, 0-3 */
    f32 angle,timeStamp;         /* data: the angle the tire has turned through, radians */
    void (*func)(void);
} tVisual;
/* A tire's state in the model record, seen from 8 bytes into MAT3's tire record (which starts at +0x430), so its
 * angvel is at +0x40 here (MAT3's +0x48). */
typedef struct TireView {
    u8 pad[64];
    f32 angvel;                  /* the tire's spin, radians a second */
    u8 tail[24];
} TireView;
/* The model record (tModelData, docs/port/CARS.md 4), the fields AnimateTire reads. */
typedef struct tModelData {
    u8 pad000[8];
    u8 body_type;                /* +0x008 the car, 0-12 */
    u8 pad009[6];
    s8 tireRims;                 /* +0x00F the rim type */
    u8 pad010[928];
    f32 steerangle;              /* +0x3B0 the front tires' steering angle, radians */
    u8 pad3B4[132];
    TireView tires[4];           /* +0x438 */
    u8 pad5A8[36];
    f32 suscomp[4];              /* +0x5CC each tire's suspension compression, feet */
    u8 pad5DC[64];
    u16 sviscode[4];             /* +0x61C each tire's surface (SURF_VIS_*) */
    u8 pad624[418];
    s16 slot;                    /* +0x7C6 */
    u8 pad7C8[4];
    s8 drone_type;               /* +0x7CC 2 a human */
    u8 tail[59];
} tModelData;
/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields AnimateTire reads. */
typedef struct tCarData {
    u8 pad000[232];
    u32 appearance;              /* +0x0E8 APPEAR_* */
    u8 pad0EC[12];
    s16 rwmph;                   /* +0x0F8 the speed, mph x 4 */
    u8 pad0FA[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 pad308[0x35C - 0x308];
    s8 camera,view,saveview,carMine;   /* +0x35C the window that follows the car (-1 none) and its view */
    u8 tail[0x380 - 0x360]; void *player; u8 tail_384[0x3B8 - 0x384];
} tCarData;
enum n64p_f32 { n64p_f32_ };
/* The car's physics descriptor (MAT3 _tCar, gCarModels[body]), its tire fields: tires[] are the tire records' 32-bit
 * window addresses (a record's first word its radius). */
typedef struct tCar {
    u8 pad000[96];
    enum n64p_f32 tires[4];      /* +0x60 */
    Vec3 tireR[4];               /* +0x70 each tire's place on the body, feet */
} tCar;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_f32: (f32 *)PC_N64P(x), default: (x))

/* game.frame_rate 60 (pc_game_fps.h, docs/port/FRAMERATE.md 7.6): the in-car view's bob, spring_save = spring_save *
 * 0.75 + noise once a frame of two retraces, taken every frame: the decay to the frame's length and the noise scaled
 * so the bob keeps the N64's spread (an AR(1) whose stationary variance is the same), instead of stepping once a 30 Hz
 * frame, a camera moving at 30 a second. At original both are the N64's. */
#define PC_SPRING_KEEP(e) PC_FILTER(e)
#define PC_SPRING_NOISE(x, e) (pc_fps60 ? (x) * pc_fps_noise(e) : (x))

/* Tire v->index of car v->slot (VIS_TIRES + tire), each frame (op 1): not shown for a hulk or a car shown as a
 * mine, and in the car's own window only in views 2 and up. The tire sits at its place on the body, down by its
 * radius and up by its suspension's compression (0 to 0.5 feet), with a random bounce on dirt that grows with the
 * speed; in the cockpit view the first tire also shakes the camera (gCamera's view2spring: less on pavement, none
 * in the air). It turns by its spin times the time since the last frame (a fixed 3.7 a frame above 64 radians a
 * second) and the front tires steer. The first tire's node gets the texture: the blurred rim (tire_blur_texture)
 * above that spin, else the rim (gWheelTextures from 3), or without the Expansion Pak on tracks 0-5 the plain ones
 * (0 and 16); MAT3 sets it on every tire. Then the tire scales of a human's car (by slot) or the drones' (row 0),
 * and it blinks with the invisible cheat or a respawn. op 0 stops it: the tire is hidden. */
void AnimateTire(tVisual *v, s16 op)
{
    f32 pos[3], mat[3][3], rpy[3], spring, spring2, t, vel, lastpos;
    tCarData *car;
    s16 tire, slot, row, mph;
    s32 lucent;
    s32 camera;
    tCar *cartp;
    tModelData *m;
    s32 show, index;

    if (op == 0) {
        MBOX_HideObject(v->objnum, 1, 15);
        v->func = 0;
        v->objnum = -1;
        return;
    }
    slot = v->slot;
    car = PC_CAR(slot);
    tire = v->index;
    m = PC_MODEL(slot);
    spring = m->suscomp[tire];
    show = gInvisibleCar != 0;
    if (!show) show = (car->appearance & APPEAR_INVISIBLE) != 0;
    lucent = show;
    mph = car->rwmph >> 2;
    camera = car->camera;
    if (m->drone_type == 2) row = m->slot + 1;
    else row = 0;
    cartp = gCarModels[m->body_type];
    show = (car->appearance & APPEAR_HULK) == 0;
    if (show) show = car->carMine == 0;
    if (!CheckVisibleAll(v, show)) return;
    CheckVisible(v, car->view >= 2, camera);
    if (PC_MODEL_VISCODE(slot)[tire] == SURF_VIS_DIRT) {
        t = (mph >> 2) * 0.05f;
        if (t > 0.6f) t = 0.6f;
        spring += Random(t);
    }
    if (spring < 0) spring = 0;
    else if (spring > 0.5f) spring = 0.5f;
    pos[0] = cartp->tireR[tire][0];
    pos[1] = cartp->tireR[tire][1] + *N64P(cartp->tires[tire]) + spring;
    pos[2] = cartp->tireR[tire][2];
    if (camera >= 0 && car->view == 1 && tire == 0) {
        t = (mph >> 2) * 0.002f;
        if (PC_MODEL_VISCODE(slot)[tire] == SURF_VIS_DIRT) spring2 = (Random(2.0f) - 1.0f) * t;
        else if (PC_MODEL_VISCODE(slot)[tire] == SURF_VIS_AIRBORNE) spring2 = 0;
        else spring2 = (Random(2.0f) - 1.0f) * t * 0.25f;
        gCamera[camera].view2spring = gCamera[camera].view2spring * PC_SPRING_KEEP(0.75f) + PC_SPRING_NOISE(spring2, 0.75f);
    }
    vel = m->tires[tire].angvel;
    lastpos = v->angle;
    if (vel > 64.0f) lastpos += PC_FSCALE(3.7f);
    else lastpos += vel * (gModelTime - v->timeStamp);
    v->angle = lastpos;
    v->timeStamp = gModelTime;
    rpy[0] = -lastpos;
    rpy[1] = (tire < 2) ? -m->steerangle : 0;
    rpy[2] = 0;
    if (tire == 0) {
        index = m->tireRims;
        if (m->tires[tire].angvel > 64.0f) {
            index = (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6) ? 0 : tire_blur_texture[index];
        } else {
            index = (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6) ? 16 : index + 3;
        }
        ZOID_SetTexture(v->objnum, (void *) (intptr_t)gWheelTextures[index], -1);
    }
    CreateYPRMatrix(mat, rpy);
    if (tire >= 2) {
        if (gRearTireScale[row][m->body_type] != 1.0f) ScaleUV(mat, mat, gRearTireScale[row][m->body_type]);
    } else {
        if (gFrontTireScale[row][m->body_type] != 1.0f) ScaleUV(mat, mat, gFrontTireScale[row][m->body_type]);
    }
    ZOID_UpdateObject(v->objnum, pos, mat);
    if (lucent) ObjInstanceList[v->objnum].flags ^= 0x80000000U;
    else ObjInstanceList[v->objnum].flags &= 0x7FFFFFFFU;
}
