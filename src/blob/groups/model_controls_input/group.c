/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (whole-program group; keep = UpdateModelControls) */
/*
 * Per-player control input into each car's model record (0x808 bytes at 0x8014A250):
 *   ReadSteeringWheel  steering (+0x720)                     -- already locked, source unchanged
 *                  except that InputRecord has `u32 map[13]` (in->steerSrc -> in->map[2])
 *   ReadGasAndBrake  throttle (+0x72C) / brake (+0x728), quantised to 1/15 (internal)
 *   UpdateModelControls  the per-player loop and the two button bytes (+0x731/+0x732) (kept)
 *   ReadShifter  manual gear selection (+0x730); a deleted static: inlined at its single call
 *                  site in UpdateModelControls, its retail remains are the caller-less `jr ra; nop`
 *                  stub at 0x800E6AE8 that this definition reproduces.
 * No arcade ancestor (arcade reads pots/shifter in mdrive.c / pots.c).  Supersedes the
 * stand-in group src/blob/groups/model_controls_input: with the real caller kept, neither callee
 * needs a stand-in caller to stay out of line.
 *
 * What closed UpdateModelControls (was 16/179 words, three loop invariants rematerialised
 * the first call coloured &gRealLevels / 2 / &gRace instead of retail's 2 / &gRace /
 * &gRealLevels): the gear code is its own function ReadShifter(st, in), written with early
 * `return`s (the autoGear test included), defined AFTER UpdateModelControls so that its stub lands at
 * 0x800E6AE8. The early returns replace `goto pick` join; the inlined copy changes the
 * web creation order and the colouring falls out right.  The next stub, stub_before_model_iteration, is not
 * the button code (writing the buttons as stub_before_model_iteration gives 18 words).
 *
 * Earlier shaping that still matters:
 *  - every button test is `gRealLevels[in->pad] & in->map[k]` (two subscripted operands keep the
 *    retail order: map load first);
 *  - throttle/brake are `volatile F32`, `gear` is `volatile s8`; `st->gear += 1;` for the
 *    up-shift, `--st->gear <= 0` for the down-shift; no `car` local;
 *  - the last test is `if (...) { st->button30 = 1; continue; } else { st->button30 = 0; }`.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    /* 0x00 */ u8 player;
    /* 0x01 */ u8 pad;
    /* 0x02 */ u8 pad2[0x18 - 2];
    /* 0x18 */ u32 map[12];
    /* 0x48 */ void *ref;
} InputRecord;                          /* 0x4C */

typedef struct {
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
    /* 0x000 */ u8 pad0[0xF8];
    /* 0x0F8 */ s16 speed;
    /* 0x0FA */ u8 padFA[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 padFA_308[0x359 - 0x308];
    /* 0x359 */ s8 f359;
    /* 0x35A */ u8 pad35A[0x380 - 0x35A]; void *input; u8 pad35A_384[0x3B8 - 0x384];
} Car;                                  /* 0x3B8 */

typedef struct {
    /* 0x0 */ u8 pad0[0xC];
    /* 0xC */ u8 throttle;
    /* 0xD */ u8 brake;
    /* 0xE */ u8 padE[2];
} PadState;                             /* 0x10 */

extern InputRecord gPlayers[];
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s16 gNumPlayers;
extern s32 gGameState;
extern s32 gPauseState;
extern s8 gCoastFlag;                   /* controls locked */
extern u32 gRealLevels[];
extern u32 gRealEdges[];
extern s32 gRace;
extern f32 gRealStick[][2];             /* analog stick per pad */
extern f32 steer_dead_zone_low, steer_dead_zone_offset, steer_dead_zone_high, steer_change_up, steer_change_down, brake_stick_dead_zone;
extern s8 gUpsideDownTrack;
extern s8 gMirrorWaves;
extern s8 gMirrorControls;
extern s8 gNoBrakes;
extern PadState gControllerData[];

void ReadSteeringWheel(CarState *st, InputRecord *in);
void ReadGasAndBrake(CarState *st, InputRecord *in);
void UpdateModelControls(void);

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
#define ReadShifter pc_func_800E6AE8
void ReadShifter(CarState *st, InputRecord *in);
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
