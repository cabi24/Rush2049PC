/* The players' controls into their cars (MAT3 controls.c UpdateModelControls, _ReadGasAndBrake,
 * _ReadSteeringWheel, _ReadShifter): each frame, each player's input record (gPlayers) becomes the car's steering,
 * throttle, brake, gear, wings and abort (tModelData.mainin), from this retrace's controller state (gRealLevels,
 * gRealEdges, gRealStick: controlsys/group.c). */
#include "game/game.h"

/* A player's input record (include/game's tPadState, under the name the prototypes give it). */
typedef struct InputRecord {
    u8 slot;                /* 0x00: the player's car */
    u8 controller;          /* 0x01 */
    u8 pad2[0x18 - 2];
    u32 controls[12];       /* 0x18: each control's button bits (CONTROL_GAS ...) */
    void *save;             /* 0x48 */
} InputRecord;

/* The car record's input fields (MAT3 tModelData: crashflag, mainin, slot). */
typedef struct CarState {
    u8 pad0[0x640];
    s8 crashflag;               /* 0x640: crashed: no shifting */
    u8 pad641[0x720 - 0x641];
    f32 wheel;                  /* 0x720: steering, -1..1 */
    u8 pad724[4];               /* 0x724: clutch */
    volatile f32 brake;         /* 0x728: 0..1 in fifteenths */
    volatile f32 throttle;      /* 0x72C: 0..1 in fifteenths */
    volatile s8 gear;           /* 0x730: -1 reverse, 1-4 */
    s8 wing;                    /* 0x731 */
    s8 abort;                   /* 0x732 */
    u8 pad733[0x7C6 - 0x733];
    s16 slot;                   /* 0x7C6 */
    u8 pad7C8[0x808 - 0x7C8];
} CarState;                     /* 0x808 */

extern InputRecord gPlayers[];

VIEW(s32, gGameState);
#define gGameState gGameState_view

/* The steering from the stick's x: with the dead-zone setting (CONTROL_VALUE_STICK_DEAD) the dead zone is taken
 * out, else the stick is cubed (fine control near the centre); quantized to 1/127; a change smaller than the
 * thresholds keeps the previous value. Mirrored or upside-down tracks reverse it (unless both, without
 * gMirrorControls). Nothing while the cars coast at the end. */
void ReadSteeringWheel(CarState *car, InputRecord *player)
{
    f32 x;
    f32 q;
    f32 d;
    f32 prev;

    if (gCoastFlag != 0) {
        car->wheel = 0.0f;
        return;
    }
    x = gRealStick[player->controller][0];
    prev = car->wheel;
    if (player->controls[CONTROL_STEER] == CONTROL_VALUE_STICK_DEAD) {
        if (x < steer_dead_zone_low) {
            f32 c = steer_dead_zone_offset;
            x += c;
        } else if (steer_dead_zone_high < x) {
            x -= steer_dead_zone_high;
        } else {
            x = 0.0;
        }
    } else {
        x = (x >= 0.0f ? x : -x) * (x >= 0.0f ? x : -x) * x;
    }
    q = x * 127.0f;
    x = (f32) (s32) (q < 0.0f ? q - 0.5f : q + 0.5f) / 127;
    d = x - prev;
    if (steer_change_up < d) {
        prev = x;
    } else if (d < steer_change_down) {
        prev = x;
    }
    if ((gUpsideDownTrack != 0 || gMirrorWaves != 0) &&
        (gUpsideDownTrack == 0 || gMirrorWaves == 0 || gMirrorControls != 0)) {
        prev = -prev;
    }
    car->wheel = prev;
}

/* The throttle and the brake, each from its control: the stick's y axis (up the gas, down the brake past a dead
 * zone), an analog trigger, or a button (full or nothing); reverse also gives full throttle; gNoBrakes (a cheat)
 * takes the brake away. Both are quantized to 1/15. Nothing while the cars coast at the end. */
void ReadGasAndBrake(CarState *car, InputRecord *player)
{
    f32 q;

    if (gCoastFlag != 0) {
        car->throttle = 0.0f;
        car->brake = 0.0f;
        return;
    }
    if (player->controls[CONTROL_GAS] == CONTROL_VALUE_STICK_Y || player->controls[CONTROL_GAS] == CONTROL_VALUE_STICK_DEAD) {
        car->throttle = gRealStick[player->controller][1];
        if (car->throttle < 0.0f) {
            car->throttle = 0.0f;
        }
    } else if (player->controls[CONTROL_GAS] == CONTROL_VALUE_ANALOG_1) {
        car->throttle = (u32) gControllerData[player->controller].analog[0] / 255.0f;
    } else if (player->controls[CONTROL_GAS] == CONTROL_VALUE_ANALOG_2) {
        car->throttle = (u32) gControllerData[player->controller].analog[1] / 255.0f;
    } else if (gRealLevels[player->controller] & player->controls[CONTROL_GAS]) {
        car->throttle = 1.0f;
    } else {
        car->throttle = 0.0f;
    }
    if (gRealLevels[player->controller] & player->controls[CONTROL_REVERSE]) {
        car->throttle = 1.0f;
    }
    if (gNoBrakes != 0) {
        car->brake = 0.0f;
    } else if (player->controls[CONTROL_BRAKE] == CONTROL_VALUE_STICK_Y || player->controls[CONTROL_BRAKE] == CONTROL_VALUE_STICK_DEAD) {
        car->brake = -gRealStick[player->controller][1];
        if (car->brake < brake_stick_dead_zone) {
            car->brake = 0.0f;
        }
    } else if (player->controls[CONTROL_BRAKE] == CONTROL_VALUE_ANALOG_1) {
        car->brake = (u32) gControllerData[player->controller].analog[0] / 255.0f;
    } else if (player->controls[CONTROL_BRAKE] == CONTROL_VALUE_ANALOG_2) {
        car->brake = (u32) gControllerData[player->controller].analog[1] / 255.0f;
    } else if (gRealLevels[player->controller] & player->controls[CONTROL_BRAKE]) {
        car->brake = 1.0f;
    } else {
        car->brake = 0.0f;
    }
    if (car->throttle * 15.0f < 0.0f) {
        q = car->throttle * 15.0f - 0.5f;
    } else {
        q = car->throttle * 15.0f + 0.5f;
    }
    car->throttle = (f32) (s32) q / 15;
    if (car->brake * 15.0f < 0.0f) {
        q = car->brake * 15.0f - 0.5f;
    } else {
        q = car->brake * 15.0f + 0.5f;
    }
    car->brake = (f32) (s32) q / 15;
}

/* The manifest owner of ReadShifter is the empty deleted-procedure stub (src/blob/ReadShifter.c), which
 * the call below binds to on PC. The gear code written here gets its own name (docs/port/PROTOTYPES.md). */
#define ReadShifter pc_ReadShifter

/* Each player's controls into their car, unless the game is paused or in its demo: throttle and brake, steering,
 * gear, and the wings (held; not in a ghost race, gRace 2: MAT3 kGhost) and abort (pressed; not in battle, gRace 6:
 * kBattle), neither while the cars coast at the end. */
void UpdateModelControls(void)
{
    InputRecord *player;
    CarState *car;
    s32 i;

    player = gPlayers;
    for (i = 0; i < gNumPlayers; i++, player++) {
        car = PC_MODEL(player->slot);
        if (gPauseState != 0 || (gGameState & GAME_STATE_DEMO)) {
            continue;
        }
        ReadGasAndBrake(car, player);
        ReadSteeringWheel(car, player);
        ReadShifter(car, player);
        if (gRace != 2 && gCoastFlag == 0 && (gRealLevels[player->controller] & player->controls[CONTROL_WINGS])) {
            car->wing = 1;
        } else {
            car->wing = 0;
        }
        if (gRace != 6 && gCoastFlag == 0 && (gRealEdges[player->controller] & player->controls[CONTROL_ABORT])) {
            car->abort = 1;
            continue;
        } else {
            car->abort = 0;
        }
    }
}

/* The gear, for a manual gearbox: nothing for a crashed or dead car, or while coasting. Reverse held: reverse.
 * Shift down pressed: one down, not below first. Shift up pressed: one up, not past fourth; from reverse (gear 0
 * then, or still reverse when not shifting up) the gear is chosen by the car's speed (the reckoned mph / 4: above
 * 110 fourth, above 80 third, above 50 second, else first). */
void ReadShifter(CarState *car, InputRecord *player)
{
    s32 speed;

    if (car->crashflag != 0) {
        return;
    }
    if (PC_CAR_DEAD(car->slot) > 0 || gCoastFlag != 0) {
        return;
    }
    if (gRealLevels[player->controller] & player->controls[CONTROL_REVERSE]) {
        car->gear = -1;
        return;
    }
    if (gRealEdges[player->controller] & player->controls[CONTROL_SHIFT_DOWN]) {
        if (--car->gear <= 0) {
            car->gear = 1;
        }
        return;
    }
    if (gRealEdges[player->controller] & player->controls[CONTROL_SHIFT_UP]) {
        car->gear += 1;
        if (car->gear >= 5) {
            car->gear = 4;
        }
        if (car->gear != 0) {
            return;
        }
    } else if (car->gear >= 0) {
        return;
    }
    speed = PC_CAR_MPH(car->slot) >> 2;
    if (speed >= 111) {
        car->gear = 4;
    } else if (speed >= 81) {
        car->gear = 3;
    } else if (speed >= 51) {
        car->gear = 2;
    } else {
        car->gear = 1;
    }
}
