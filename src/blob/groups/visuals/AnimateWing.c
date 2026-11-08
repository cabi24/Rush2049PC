/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AnimateWing (0x800924F4, 419 words): Visual callback (v, op) for a per-car part that grows/shrinks in six
 * timed steps (flags 0x100 grow, 0x200 shrink, 0x1000 active, 0x10 shown), with an engine-ish sound handle
 * on the model record (start: AudPlayEffect(61,...) + volume 0.75 + AudChangeEffectVol 0.5; stop:
 * AudStopEffect), basis rotation from the car pose, and the CheckVisible/CheckXlu tail shared with the
 * locked sibling AnimateWingFlame. No exact arcade ancestor (AnimateTire is the closest shape).
 * STATE: unit EQUAL ("locked bodies that differ in this unit: 0") and MATCH as a member of the
 * superseding group visual_tire_module (the scorer, own.rodata verified). It is not a
 *   standalone match: AudChangeEffect's IPA float-register convention and the register-parameter
 *   CheckVisibleAll/CheckVisible need the group context.
 * Shaping devices / hypotheses (all disclosed; helper identities unknown):
 *   - ehu_inline: byte-identical copy of the locked AudChangeEffectPitch body as a cfe `__inline` function
 *     (retail inlines it twice; umerge does not inline the kept locked one). Renamed so that the locked symbol
 *     is not redefined.
 *   - tire_extend / tire_sound_on / tire_sound_off: `__inline` helpers. They give retail's inline-area layout
 *     (sound handle homes 136/112). Block 1's sound start is written in place; block 2 is
 *     `tire_extend(v, model); if (model->sound == -1) tire_sound_on(model);`.
 *     The `if` sits in the caller, and tire_extend does not call tire_sound_on, because of the group
 * compile. umerge drops the Uloc records of a leaf inlinee. A nested inlinee (one that itself
 *     inlined something) keeps them in a small group compile but not in the whole-program unit. The two
 *     closing-brace Ulocs of a nested inlinee that ends at an if-join split that join, and PRE then
 *     hoists v->flags (283 words off in the group).
 *   - pX, pY[3]: unused locals = exact named-frame residual (retail position at sp+208, reverse at sp+192:
 *     one word between sign and position, three between position and reverse). Frame 240 with them,
 *     224 without (4 slots, 16 B; unit `frame 240/224`).
 *   - model = (Model2056 *)(u32)&D_8014A250[slot]: launder (w9 technique) so the position[0] store kills
 *     the model->type load as in retail (lbu reload + multu by a hoisted 12). It is assigned before
 *     `reverse` so that its expression web precedes the v->flags web in f_spilltemps order (home sp+92).
 *   - `(0, sign)`: comma-operator device for retail's `mul.s $f6,$f0,$f4` (sign first). Every plain spelling
 *     tried gives mpy(load, sign): both source orders, +sign, (f32)sign, sign*1.0f, register, a tbl pointer.
 *     An expression-for-sign gives the right code, but without the named `sign` the frame comes out one slot
 *     wrong.
 *   - Time updates are written `v->time = gModelTime + 0.015f;` before the index update (as1 order).
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16;
typedef unsigned short u16; typedef int s32; typedef unsigned int u32; typedef float f32;
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Visual { void *opaque0; s16 index, object, slot, opaque10; u32 flags; f32 time; void (*func)(struct Visual *v, s16 op); } Visual;
typedef struct Model2056 { u8 opaque0[8]; u8 type; u8 opaque9[1007]; s16 state; u8 opaque1018[2]; s32 sound; u8 opaque1024[966]; s16 player; u8 opaque1992[64]; } Model2056;
typedef struct Pose { u8 opaque0[20]; f32 angle; } Pose;
typedef struct Player952 { u8 opaque0[232]; u32 flags; u8 opaque236[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 opaque236_308[0x35C - 0x308]; s8 shift, quality, opaque862, disabled; u8 opaque864[32]; Pose *pose; u8 opaque900[52]; } Player952;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Model68 { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Model68;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Model68 ObjInstanceList[];
extern f32 wingOffsets[][3];
extern f32 unitmatrix[9];
extern f32 gModelTime;
extern s8 gCoastFlag;
extern s8 gInvisibleCar;
extern s16 gNumPlayers;
void MBOX_HideObject(s32 a, s32 mode, u32 v);
void MBOX_ShowObject(s32 a, s32 mode, u32 v);
u32 AudPlayEffect(u32 index, u32 other, u32 value, u8 mode);
void AudChangeEffect(s32 h, f32 x, f32 y, f32 z, f32 w);
s32 osRecvMesg(void *, void *, s32);
s32 osSendMesg(void *, void *, s32);
extern s32 audio_lock_queue;
__inline void ehu_inline(s32 h, f32 value)
{
    osRecvMesg(&audio_lock_queue, 0, 1);
    AudChangeEffect(h, -2.0f, -2.0f, -2.0f, value);
    osSendMesg(&audio_lock_queue, 0, 0);
}
void AudChangeEffectVol(s32 h, f32 opacity);
void AudStopEffect(s32 h);
void InitWingSmoke(s16 arg0, s16 arg1);
void CopyUV(void *arg0, void *arg1);
void PitchUV(f32 angle, f32 uv[][3]);
void ZOID_UpdateObject(s16 arg0, void *arg1, void *arg2);
void StartSmoke(s16 slot, u32 type, s32 fast, f32 *offset);
s32 CheckVisibleAll(Visual *v, s32 on);
s32 CheckVisible(Visual *v, s32 show, s32 part);

/* PC: game.frame_rate 60 (pc_game_fps.h). The wings step out and in one sixth at a frame whose model time is past
 * v->time, set 0.015 s on: less than a frame, so a step a frame, 30 a second on the N64. At 60 the gap is 0.03 s,
 * between one frame and two, so the steps keep the N64's 30 a second. */
#define PC_GAP30(x) (pc_fps60 ? (x) * 2.0f : (x))
__inline void tire_sound_on(Model2056 *model)
{
    model->sound = AudPlayEffect(61, model->player, 2, 2);
    ehu_inline(model->sound, 0.75f);
    AudChangeEffectVol(model->sound, 0.5f);
}
__inline void tire_extend(Visual *v, Model2056 *model)
{
    MBOX_ShowObject(v->object, 0, 15);
    v->flags |= 0x1110;
    v->time = gModelTime + PC_GAP30(0.015f); v->index = 0;
}
__inline void tire_sound_off(Model2056 *model)
{
    if (model->sound != -1) {
        AudStopEffect(model->sound);
        model->sound = -1;
    }
}

void AnimateWing(Visual *v, s16 op)
{
    s32 hulk, lucent;
    s16 slot;
    f32 sign;
    f32 pX;
    f32 position[3];
    f32 pY[3];
    s32 reverse;
    f32 basis[3][3];
    Model2056 *model;
    Player952 *car;

    slot = v->slot;
    if (op == 0) {
        if (v->object >= 0) MBOX_HideObject(v->object, 0, 15);
        v->func = 0;
        v->object = -1;
        return;
    }
    model = (Model2056 *)(uintptr_t)PC_MODEL(slot);
    reverse = (v->flags & 1) ? 1 : 0;
    sign = reverse ? 1.0f : -1.0f;
    position[0] = wingOffsets[model->type][0] * (0, sign);
    position[1] = wingOffsets[model->type][1];
    position[2] = 0.0f;
    if (model->state == 0 || gCoastFlag) {
        if (v->flags & 0x10) {
            v->flags &= ~0x100;
            v->flags |= 0x1200;
            if (model->sound == -1) {
                model->sound = AudPlayEffect(61, model->player, 2, 2);
                ehu_inline(model->sound, 0.75f);
                AudChangeEffectVol(model->sound, 0.5f);
            }
        } else if (!(v->flags & 0x1000)) {
            InitWingSmoke(slot, reverse);
            MBOX_HideObject(v->object, 0, 15);
            return;
        }
    }
    if (model->state != 0 && !(v->flags & 0x10)) {
        tire_extend(v, model);
        if (model->sound == -1) tire_sound_on(model);
    }
    if (v->flags & 0x1000) {
        if (v->flags & 0x100) {
grow:
            if (v->time < gModelTime) {
                position[0] *= (f32)v->index / 6.0f;
                ZOID_UpdateObject(v->object, position, 0);
                v->time = gModelTime + PC_GAP30(0.015f); v->index++;
                if (v->index == 6) {
                    v->flags &= ~0x1100;
                    tire_sound_off(model);
                }
            }
        } else if (v->flags & 0x200) {
            if (model->state != 0 && !gCoastFlag) {
                v->flags &= ~0x200;
                v->flags |= 0x100;
                goto grow;
            }
            if (v->time < gModelTime) {
                position[0] *= (f32)v->index / 6.0f;
                ZOID_UpdateObject(v->object, position, 0);
                v->time = gModelTime + PC_GAP30(0.015f); v->index--;
                if (v->index == -1) {
                    v->flags &= ~0x1210;
                    tire_sound_off(model);
                }
            }
        }
    }
    if (v->flags & 0x10) {
        CopyUV(unitmatrix, basis);
        PitchUV(PC_CAR_INPUT_AS(Pose, slot)->angle * 0.507f, basis);
        ZOID_UpdateObject(v->object, 0, basis);
        if (gNumPlayers == 1) StartSmoke(slot, reverse ? 7 : 6, 0, position);
    }
    car = PC_CAR(slot);
    hulk = (car->flags & 0x10) != 0;
    lucent = (gInvisibleCar != 0) || ((car->flags & 8) != 0);
    if (CheckVisibleAll(v, !hulk && !car->disabled)) {
        CheckVisible(v, car->quality >= 2, car->shift);
        if (lucent) ObjInstanceList[v->object].flags ^= 0x80000000;
        else ObjInstanceList[v->object].flags &= 0x7FFFFFFF;
    }
}
