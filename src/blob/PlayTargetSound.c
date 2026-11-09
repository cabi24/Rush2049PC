#include "game/game.h"

typedef struct GameCar { s32 header[2]; f32 position[3]; s32 middle[6]; f32 matrix[9]; unsigned char rest[0x110 - 0x50]; struct pc_visual visuals[21]; unsigned char rest_308[0x380 - 0x308]; void *input; unsigned char rest_384[0x3B8 - 0x384]; } GameCar;

#define vecsub(a,b,r) {r[0]=a[0]-b[0]; r[1]=a[1]-b[1]; r[2]=a[2]-b[2];}
s32 PlayTargetSound(s32 sound, s32 slot, f32 *position, u8 mode) {
    GameCar *car = PC_CAR(slot);
    f32 output[3];
    f32 delta[3];
    f32 threshold;
    f32 magnitude_squared;
    f32 side_squared;
    f32 forward_squared;
    s32 direction;

    if (gGameSoundsEnabled == 0) {
        return -1;
    }
    if (PC_LINK_OWNER(slot) != 6) {
        return -1;
    }
    vecsub(position, car->position, delta);
    BodyVector(delta, output, car->matrix);
    forward_squared = output[2] * output[2];
    side_squared = output[0] * output[0];
    direction = 0;
    magnitude_squared = forward_squared + side_squared;
    if (magnitude_squared > 1.0f) {
        forward_squared *= (f32)(output[2] >= 0.0f ? 1 : -1);
        side_squared *= (f32)(output[0] >= 0.0f ? 1 : -1);
        threshold = (.924f * .924f) * magnitude_squared;
        if (threshold < side_squared) {
            direction = 4;
        } else if (side_squared < -threshold) {
            direction = 8;
        }
        if (threshold < forward_squared) {
            direction |= 1;
        } else if (forward_squared < -threshold) {
            direction |= 2;
        }
    }
    if (sound == 6) {
        return AudPlayEffectVP(sound, slot, 1, mode, 1.0f, 0.0f, 0.0f);
    }
    return AudPlayEffectVP(sound, slot, 1, mode, 1.0f, (f32) gBumpLRTab[direction], (f32) gBumpFBTab[direction]);
}
