/* The cars' wings (MAT3 visuals.c AnimateWing): a car visual that slides the wing out (six steps) while the
 * wings control is held, with its sound, and back in, tilting it with the car's pitch and puffing smoke. */
#include "game/game.h"

/* PC: a visual (tVisual) is 0x28 bytes: next, data and func are pointer-sized (struct pc_visual, docs/port/CARS.md 3);
 * a wing keeps its flags in the data word. */
typedef struct Visual { void *opaque0; s16 index, object, slot, opaque10; uintptr_t flags; f32 time; void (*func)(struct Visual *v, s16 op); } Visual;
typedef struct Model2056 { _tCar *car; Tune44 *tune; u8 type; u8 opaque9[919]; streetyp *lasttp[4]; u8 opaque944[72]; s16 state; u8 opaque1018[2]; s32 sound; u8 opaque1024[12]; const s16 *torquecurve; u8 opaque1040[568]; surftyp *movingSurface; u8 opaque1612[378]; s16 player; u8 opaque1992[64]; } Model2056;
typedef struct Pose { u8 opaque0[20]; f32 angle; } Pose;
typedef struct Player952 { u8 opaque0[232]; u32 flags; u8 opaque236[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 opaque236_308[0x35C - 0x308]; s8 shift, quality, opaque862, disabled; u8 opaque864[32]; Pose *pose; u8 opaque900[52]; } Player952;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */

/* PC: game.frame_rate 60 (pc_game_fps.h). The wings step out and in one sixth at a frame whose model time is past
 * v->time, set 0.015 s on: less than a frame, so a step a frame, 30 a second on the N64. At 60 the gap is 0.03 s,
 * between one frame and two, so the steps keep the N64's 30 a second. */
#define PC_GAP30(x) (pc_fps60 ? (x) * 2.0f : (x))

/* The wing's sound (effect 61 at three quarters pitch, half volume), on the car's player. */
__inline void wing_sound_on(Model2056 *model)
{
    model->sound = AudPlayEffect(61, model->player, 2, 2);
    AudChangeEffectPitch(model->sound, 0.75f);
    AudChangeEffectVol(model->sound, 0.5f);
}
/* Starts the wing sliding out: shown, flags out-and-moving (0x1110), the first step a moment from now. */
__inline void wing_extend(Visual *v, Model2056 *model)
{
    MBOX_ShowObject(v->object, 0, 15);
    v->flags |= 0x1110;
    v->time = gModelTime + PC_GAP30(0.015f); v->index = 0;
}
/* Stops the wing's sound. */
__inline void wing_sound_off(Model2056 *model)
{
    if (model->sound != -1) {
        AudStopEffect(model->sound);
        model->sound = -1;
    }
}

/* The wing visual v of a car, each frame (op != 0), or its removal (op 0). The flags: 0x10 the wing is out (or
 * going out), 0x100 sliding out, 0x200 sliding in, 0x1000 moving. The wing goes in when the wings control is let
 * go (model state 0) or the car coasts at the end, and out when it is held: a step every 0.015 s of model time
 * (two at 60 fps), its offset (wingOffsets by car type, to the side the visual mirrors) scaled by the step; the
 * sound plays while it moves. While out it tilts with the car's pitch and, for one player, smokes. It is hidden
 * with the car (a hulk, a disabled car), at low detail, and see-through with the car. */
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
    model = (Model2056 *)PC_MODEL(slot);
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
                AudChangeEffectPitch(model->sound, 0.75f);
                AudChangeEffectVol(model->sound, 0.5f);
            }
        } else if (!(v->flags & 0x1000)) {
            InitWingSmoke(slot, reverse);
            MBOX_HideObject(v->object, 0, 15);
            return;
        }
    }
    if (model->state != 0 && !(v->flags & 0x10)) {
        wing_extend(v, model);
        if (model->sound == -1) wing_sound_on(model);
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
                    wing_sound_off(model);
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
                    wing_sound_off(model);
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
