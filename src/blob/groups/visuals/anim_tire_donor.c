#include "game/game.h"

/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* AnimateTire: direct ancestor rushtherock/game/visuals.c:AnimateTire (suscomp spring, DIRT bounce with
 * Random, spring_save, wheel object swap at vel 64, rpy, CheckXlu). CheckVisibleAll is arcade
 * CheckVisible(Visual *v, BOOL on) and CheckVisible its N64 per-part variant; Random is Random()
 * (inlined by umerge).
 * Shaping and disclosures:
 * - `s16 i, j` (arcade AnimateTire's declaration list `S16 tire, i,j, slot, ...`) are unused. They are the exact
 *   home-layout residual: retail homes tire@138 slot@132 row@130 mph@128 lucent@124 controller@120 config@116;
 *   without them every s16/word home above the spill area moves up by 4 (frame stays 224 by 8-byte rounding,
 *   30 differing rows in the unit); with only `i`, 20 rows. Two 2-byte slots.
 * - The translucency flag is computed in `show` and copied to `lucent` (`show` is reused for the visibility test):
 *   retail keeps the flag in a0 only up to the row test and stores lucent there (uopt cannot copy-propagate
 * `lucent = show` because show is redefined). Traced with the split-instrumented uopt.
 * - No compiled-out reads, no stand-ins, no implicit int.
 */
#ifndef VISUAL_TIRE_TYPES_H
#define VISUAL_TIRE_TYPES_H

typedef f32 Vec3[3];

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Visual {
    void *prefix;
    s16 tire,object,slot,reserved;
    f32 angle,time;
    void (*callback)(void);
} Visual;
typedef struct Tire92 {
    u8 prefix[64];
    f32 angular_velocity;
    u8 tail[24];
} Tire92;
typedef struct Model2056 {
    u8 prefix[8];
    u8 car_type;
    u8 to_body_type[6];
    s8 body_type;
    u8 to_steering[928];
    f32 steering;
    u8 to_tires[132];
    Tire92 tires[4];
    u8 to_suspension[36];
    f32 suspension[4];
    u8 to_visual_code[64];
    u16 visual_code[4];
    u8 to_player[418];
    s16 player;
    u8 to_mode[4];
    s8 mode;
    u8 tail[59];
} Model2056;
typedef struct Car952 {
    u8 prefix[232];
    u32 appearance;
    u8 to_mph[12];
    s16 mph;
    u8 to_controller[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 to_controller_308[0x35C - 0x308];
    s8 controller,render_state,reserved,hidden;
    u8 tail[0x380 - 0x360]; void *input; u8 tail_384[0x3B8 - 0x384];
} Car952;
enum n64p_f32 { n64p_f32_ };
typedef struct CarConfig {
    u8 prefix[96];
    enum n64p_f32 radius[4];
    Vec3 tire_position[4];
} CarConfig;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_f32: (f32 *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
typedef struct Control152 {
    u8 prefix[144];
    f32 spring_save;
    u8 tail[4];
} Control152;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Object68 {
    u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env;
} Object68;
VIEW(CarConfig *[], gCarModels);
#define gCarModels gCarModels_view
VIEW(Control152[], gCamera);
#define gCamera gCamera_view
VIEW(Object68[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

VIEW(intptr_t[], gWheelTextures);
#define gWheelTextures gWheelTextures_view

extern void ZOID_SetTexture(s16,intptr_t,s32);

#endif

/* game.frame_rate 60 (pc_game_fps.h, docs/port/FRAMERATE.md 7.6): the in-car view's bob, spring_save = spring_save *
 * 0.75 + noise once a frame of two retraces, taken every frame: the decay to the frame's length and the noise scaled
 * so the bob keeps the N64's spread (an AR(1) whose stationary variance is the same), instead of stepping once a 30 Hz
 * frame, a camera moving at 30 a second. At original both are the N64's. */
#define PC_SPRING_KEEP(e) PC_FILTER(e)
#define PC_SPRING_NOISE(x, e) (pc_fps60 ? (x) * pc_fps_noise(e) : (x))
void AnimateTire(Visual *v, s16 op)
{
    f32 pos[3], mat[3][3], rpy[3], spring, spring2, t, vel, lastpos;
    Car952 *car;
    s16 tire, i, j, slot, row, mph;
    s32 lucent;
    s32 controller;
    CarConfig *config;
    Model2056 *m;
    s32 show, index;

    if (op == 0) {
        MBOX_HideObject(v->object, 1, 15);
        v->callback = 0;
        v->object = -1;
        return;
    }
    slot = v->slot;
    car = PC_CAR(slot);
    tire = v->tire;
    m = PC_MODEL(slot);
    spring = m->suspension[tire];
    show = gInvisibleCar != 0;
    if (!show) show = (car->appearance & 8) != 0;
    lucent = show;
    mph = car->mph >> 2;
    controller = car->controller;
    if (m->mode == 2) row = m->player + 1;
    else row = 0;
    config = gCarModels[m->car_type];
    show = (car->appearance & 16) == 0;
    if (show) show = car->hidden == 0;
    if (!CheckVisibleAll(v, show)) return;
    CheckVisible(v, car->render_state >= 2, controller);
    if (PC_MODEL_VISCODE(slot)[tire] == 1) {
        t = (mph >> 2) * 0.05f;
        if (t > 0.6f) t = 0.6f;
        spring += Random(t);
    }
    if (spring < 0) spring = 0;
    else if (spring > 0.5f) spring = 0.5f;
    pos[0] = config->tire_position[tire][0];
    pos[1] = config->tire_position[tire][1] + *N64P(config->radius[tire]) + spring;
    pos[2] = config->tire_position[tire][2];
    if (controller >= 0 && car->render_state == 1 && tire == 0) {
        t = (mph >> 2) * 0.002f;
        if (PC_MODEL_VISCODE(slot)[tire] == 1) spring2 = (Random(2.0f) - 1.0f) * t;
        else if (PC_MODEL_VISCODE(slot)[tire] == 8) spring2 = 0;
        else spring2 = (Random(2.0f) - 1.0f) * t * 0.25f;
        gCamera[controller].spring_save = gCamera[controller].spring_save * PC_SPRING_KEEP(0.75f) + PC_SPRING_NOISE(spring2, 0.75f);
    }
    vel = m->tires[tire].angular_velocity;
    lastpos = v->angle;
    if (vel > 64.0f) lastpos += PC_FSCALE(3.7f);
    else lastpos += vel * (gModelTime - v->time);
    v->angle = lastpos;
    v->time = gModelTime;
    rpy[0] = -lastpos;
    rpy[1] = (tire < 2) ? -m->steering : 0;
    rpy[2] = 0;
    if (tire == 0) {
        index = m->body_type;
        if (m->tires[tire].angular_velocity > 64.0f) {
            index = (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6) ? 0 : tire_blur_texture[index];
        } else {
            index = (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6) ? 16 : index + 3;
        }
        ZOID_SetTexture(v->object, gWheelTextures[index], -1);
    }
    CreateYPRMatrix(mat, rpy);
    if (tire >= 2) {
        if (gRearTireScale[row][m->car_type] != 1.0f) ScaleUV(mat, mat, gRearTireScale[row][m->car_type]);
    } else {
        if (gFrontTireScale[row][m->car_type] != 1.0f) ScaleUV(mat, mat, gFrontTireScale[row][m->car_type]);
    }
    ZOID_UpdateObject(v->object, pos, mat);
    if (lucent) ObjInstanceList[v->object].flags ^= 0x80000000U;
    else ObjInstanceList[v->object].flags &= 0x7FFFFFFFU;
}
