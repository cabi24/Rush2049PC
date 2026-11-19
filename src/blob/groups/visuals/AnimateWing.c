/* The cars' wings (MAT3 visuals.c AnimateWing): a car visual that slides the wing out (six steps) while the
 * wings control is held, with its sound, and back in, tilting it with the car's pitch and puffing smoke. */
#include "game/game.h"


/* PC: game.frame_rate 60 (pc_game_fps.h). The wings step out and in one sixth at a frame whose model time is past
 * v->time, set 0.015 s on: less than a frame, so a step a frame, 30 a second on the N64. At 60 the gap is 0.03 s,
 * between one frame and two, so the steps keep the N64's 30 a second. */
#define PC_GAP30(x) (pc_fps60 ? (x) * 2.0f : (x))

/* The wing's sound (effect 61 at three quarters pitch, half volume), on the car's player. */
__inline void wing_sound_on(tModelData *model)
{
    model->wingSnd = AudPlayEffect(61, model->slot, 2, 2);
    AudChangeEffectPitch(model->wingSnd, 0.75f);
    AudChangeEffectVol(model->wingSnd, 0.5f);
}
/* Starts the wing sliding out: shown, flags out-and-moving (0x1110), the first step a moment from now. */
__inline void wing_extend(tVisual *v, tModelData *model)
{
    MBOX_ShowObject(v->objnum, 0, 15);
    v->data |= 0x1110;
    v->timeStamp = gModelTime + PC_GAP30(0.015f); v->index = 0;
}
/* Stops the wing's sound. */
__inline void wing_sound_off(tModelData *model)
{
    if (model->wingSnd != -1) {
        AudStopEffect(model->wingSnd);
        model->wingSnd = -1;
    }
}

/* The wing visual v of a car, each frame (op != 0), or its removal (op 0). The flags: 0x10 the wing is out (or
 * going out), 0x100 sliding out, 0x200 sliding in, 0x1000 moving. The wing goes in when the wings control is let
 * go (model state 0) or the car coasts at the end, and out when it is held: a step every 0.015 s of model time
 * (two at 60 fps), its offset (wingOffsets by car type, to the side the visual mirrors) scaled by the step; the
 * sound plays while it moves. While out it tilts with the car's pitch and, for one player, smokes. It is hidden
 * with the car (a hulk, a disabled car), at low detail, and see-through with the car. */
void AnimateWing(tVisual *v, s16 op)
{
    s32 hulk, lucent;
    s16 slot;
    f32 sign;
    f32 pX;
    f32 position[3];
    f32 pY[3];
    s32 reverse;
    f32 basis[3][3];
    tModelData *model;
    tCarData *car;

    slot = v->slot;
    if (op == 0) {
        if (v->objnum >= 0) MBOX_HideObject(v->objnum, 0, 15);
        v->func = 0;
        v->objnum = -1;
        return;
    }
    model = &pc_models[slot];
    reverse = (v->data & 1) ? 1 : 0;
    sign = reverse ? 1.0f : -1.0f;
    position[0] = wingOffsets[model->body_type][0] * (0, sign);
    position[1] = wingOffsets[model->body_type][1];
    position[2] = 0.0f;
    if (model->wing == 0 || gCoastFlag) {
        if (v->data & 0x10) {
            v->data &= ~0x100;
            v->data |= 0x1200;
            if (model->wingSnd == -1) {
                model->wingSnd = AudPlayEffect(61, model->slot, 2, 2);
                AudChangeEffectPitch(model->wingSnd, 0.75f);
                AudChangeEffectVol(model->wingSnd, 0.5f);
            }
        } else if (!(v->data & 0x1000)) {
            InitWingSmoke(slot, reverse);
            MBOX_HideObject(v->objnum, 0, 15);
            return;
        }
    }
    if (model->wing != 0 && !(v->data & 0x10)) {
        wing_extend(v, model);
        if (model->wingSnd == -1) wing_sound_on(model);
    }
    if (v->data & 0x1000) {
        if (v->data & 0x100) {
grow:
            if (v->timeStamp < gModelTime) {
                position[0] *= (f32)v->index / 6.0f;
                ZOID_UpdateObject(v->objnum, position, 0);
                v->timeStamp = gModelTime + PC_GAP30(0.015f); v->index++;
                if (v->index == 6) {
                    v->data &= ~0x1100;
                    wing_sound_off(model);
                }
            }
        } else if (v->data & 0x200) {
            if (model->wing != 0 && !gCoastFlag) {
                v->data &= ~0x200;
                v->data |= 0x100;
                goto grow;
            }
            if (v->timeStamp < gModelTime) {
                position[0] *= (f32)v->index / 6.0f;
                ZOID_UpdateObject(v->objnum, position, 0);
                v->timeStamp = gModelTime + PC_GAP30(0.015f); v->index--;
                if (v->index == -1) {
                    v->data &= ~0x1210;
                    wing_sound_off(model);
                }
            }
        }
    }
    if (v->data & 0x10) {
        CopyUV(unitmatrix, basis);
        PitchUV(((tPadState *)pc_cars[slot].player)->analog[1] * 0.507f, basis);
        ZOID_UpdateObject(v->objnum, 0, basis);
        if (gNumPlayers == 1) StartSmoke(slot, reverse ? 7 : 6, 0, position);
    }
    car = &pc_cars[slot];
    hulk = (car->appearance & 0x10) != 0;
    lucent = (gInvisibleCar != 0) || ((car->appearance & 8) != 0);
    if (CheckVisibleAll(v, !hulk && !car->carMine)) {
        CheckVisible(v, car->view >= 2, car->camera);
        if (lucent) ObjInstanceList[v->objnum].flags ^= 0x80000000;
        else ObjInstanceList[v->objnum].flags &= 0x7FFFFFFF;
    }
}
