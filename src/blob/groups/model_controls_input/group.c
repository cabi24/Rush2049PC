#include "game/game.h"

typedef struct InputRecord {
    /* 0x00 */ u8 player;
    /* 0x01 */ u8 pad;
    /* 0x02 */ u8 pad2[0x18 - 2];
    /* 0x18 */ u32 map[12];
    /* 0x48 */ void *ref;
} InputRecord;                          /* 0x4C */

typedef struct CarState {
    /* 0x000 */ u8 pad0[0x640];
    /* 0x640 */ s8 autoGear;
    /* 0x641 */ u8 pad641[0x720 - 0x641];
    /* 0x720 */ f32 steer;
    /* 0x724 */ u8 pad724[4];
    /* 0x728 */ volatile f32 brake;
    /* 0x72C */ volatile f32 throttle;
    /* 0x730 */ volatile s8 gear;
    /* 0x731 */ s8 button3C;
    /* 0x732 */ s8 button30;
    /* 0x733 */ u8 pad733[0x7C6 - 0x733];
    /* 0x7C6 */ s16 car;
    /* 0x7C8 */ u8 pad7C8[0x808 - 0x7C8];
} CarState;                             /* 0x808 */

typedef struct {
    /* 0x0 */ u8 pad0[0xC];
    /* 0xC */ u8 throttle;
    /* 0xD */ u8 brake;
    /* 0xE */ u8 padE[2];
} PadState;                             /* 0x10 */

extern InputRecord gPlayers[];

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(PadState[], gControllerData);
#define gControllerData gControllerData_view

/* steering */
void ReadSteeringWheel(CarState *st, InputRecord *in)
{
    f32 x;
    f32 q;
    f32 d;
    f32 prev;

    if (gCoastFlag != 0) {
        st->steer = 0.0f;
        return;
    }
    x = gRealStick[in->pad][0];
    prev = st->steer;
    if (in->map[2] == 0x19) {
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
    st->steer = prev;
}

/* throttle and brake, quantized to 1/15 */
void ReadGasAndBrake(CarState *st, InputRecord *in)
{
    f32 q;

    if (gCoastFlag != 0) {
        st->throttle = 0.0f;
        st->brake = 0.0f;
        return;
    }
    if (in->map[0] == 0x13 || in->map[0] == 0x19) {
        st->throttle = gRealStick[in->pad][1];
        if (st->throttle < 0.0f) {
            st->throttle = 0.0f;
        }
    } else if (in->map[0] == 0x15) {
        st->throttle = (u32) gControllerData[in->pad].throttle / 255.0f;
    } else if (in->map[0] == 0x16) {
        st->throttle = (u32) gControllerData[in->pad].brake / 255.0f;
    } else if (gRealLevels[in->pad] & in->map[0]) {
        st->throttle = 1.0f;
    } else {
        st->throttle = 0.0f;
    }
    if (gRealLevels[in->pad] & in->map[5]) {
        st->throttle = 1.0f;
    }
    if (gNoBrakes != 0) {
        st->brake = 0.0f;
    } else if (in->map[1] == 0x13 || in->map[1] == 0x19) {
        st->brake = -gRealStick[in->pad][1];
        if (st->brake < brake_stick_dead_zone) {
            st->brake = 0.0f;
        }
    } else if (in->map[1] == 0x15) {
        st->brake = (u32) gControllerData[in->pad].throttle / 255.0f;
    } else if (in->map[1] == 0x16) {
        st->brake = (u32) gControllerData[in->pad].brake / 255.0f;
    } else if (gRealLevels[in->pad] & in->map[1]) {
        st->brake = 1.0f;
    } else {
        st->brake = 0.0f;
    }
    if (st->throttle * 15.0f < 0.0f) {
        q = st->throttle * 15.0f - 0.5f;
    } else {
        q = st->throttle * 15.0f + 0.5f;
    }
    st->throttle = (f32) (s32) q / 15;
    if (st->brake * 15.0f < 0.0f) {
        q = st->brake * 15.0f - 0.5f;
    } else {
        q = st->brake * 15.0f + 0.5f;
    }
    st->brake = (f32) (s32) q / 15;
}

/* The manifest owner of ReadShifter is the empty deleted-procedure stub (src/blob/ReadShifter.c), which
 * the call below binds to on PC. The gear code written here gets its own name (docs/port/PROTOTYPES.md). */
#define ReadShifter pc_ReadShifter

void UpdateModelControls(void)
{
    InputRecord *in;
    CarState *st;
    s32 i;

    in = gPlayers;
    for (i = 0; i < gNumPlayers; i++, in++) {
        st = PC_MODEL(in->player);
        if (gPauseState != 0 || (gGameState & 8)) {
            continue;
        }
        ReadGasAndBrake(st, in);
        ReadSteeringWheel(st, in);
        ReadShifter(st, in);
        if (gRace != 2 && gCoastFlag == 0 && (gRealLevels[in->pad] & in->map[9])) {
            st->button3C = 1;
        } else {
            st->button3C = 0;
        }
        if (gRace != 6 && gCoastFlag == 0 && (gRealEdges[in->pad] & in->map[6])) {
            st->button30 = 1;
            continue;
        } else {
            st->button30 = 0;
        }
    }
}

void ReadShifter(CarState *st, InputRecord *in)
{
    s32 speed;

    if (st->autoGear != 0) {
        return;
    }
    if (PC_CAR_STATE(st->car) > 0 || gCoastFlag != 0) {
        return;
    }
    if (gRealLevels[in->pad] & in->map[5]) {
        st->gear = -1;
        return;
    }
    if (gRealEdges[in->pad] & in->map[4]) {
        if (--st->gear <= 0) {
            st->gear = 1;
        }
        return;
    }
    if (gRealEdges[in->pad] & in->map[3]) {
        st->gear += 1;
        if (st->gear >= 5) {
            st->gear = 4;
        }
        if (st->gear != 0) {
            return;
        }
    } else if (st->gear >= 0) {
        return;
    }
    speed = PC_CAR_MPH(st->car) >> 2;
    if (speed >= 111) {
        st->gear = 4;
    } else if (speed >= 81) {
        st->gear = 3;
    } else if (speed >= 51) {
        st->gear = 2;
    } else {
        st->gear = 1;
    }
}
