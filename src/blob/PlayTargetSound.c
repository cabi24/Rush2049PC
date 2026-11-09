/* PlayTargetSound: a track object's sound (a cone, a window, a mine ... hit), panned around the car that hit it
 * (MAT3 audio.c). */
#include "game/game.h"

/* The car record's fields this file reads (MAT3 tCarData: dr_pos, dr_uvs), in the PC's layout. */
typedef struct GameCar { s32 header[2]; f32 position[3]; s32 middle[6]; f32 matrix[9]; unsigned char rest[0x110 - 0x50]; struct pc_visual visuals[21]; unsigned char rest_308[0x380 - 0x308]; void *input; unsigned char rest_384[0x3B8 - 0x384]; } GameCar;

/* The effect that always plays centred, without the direction's pans (MAT3: 59). */
#define SFX_TARGET_CENTRED 6
/* cos(22.5 degrees): within 22.5 degrees of an axis, the sound comes from that side only. */
#define DIRECTION_COS 0.924f

#define vecsub(a,b,r) {r[0]=a[0]-b[0]; r[1]=a[1]-b[1]; r[2]=a[2]-b[2];}

/* Plays effect fxid for the car in slot with the given priority, at full volume, panned by where position lies
 * from the car in its own frame: the direction bits (4 toward +x, 8 toward -x, 1 toward +z, 2 toward -z; none
 * within one unit, and none on an axis more than 22.5 degrees off) index the pans of gBumpLRTab and gBumpFBTab.
 * Returns the fx tag handle, or -1 when the game's sounds are off or the slot has no human's view. */
s32 PlayTargetSound(s32 fxid, s32 slot, f32 *position, u8 priority) {
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
    if (PC_LINK_OWNER(slot) != LINK_OWNER_HUMAN) {
        return -1;
    }
    vecsub(position, car->position, delta);
    BodyVector(delta, output, car->matrix);
    forward_squared = output[2] * output[2];
    side_squared = output[0] * output[0];
    direction = 0;
    magnitude_squared = forward_squared + side_squared;
    if (magnitude_squared > 1.0f) {
        /* squares carrying their component's sign */
        forward_squared *= (f32)(output[2] >= 0.0f ? 1 : -1);
        side_squared *= (f32)(output[0] >= 0.0f ? 1 : -1);
        threshold = (DIRECTION_COS * DIRECTION_COS) * magnitude_squared;
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
    if (fxid == SFX_TARGET_CENTRED) {
        return AudPlayEffectVP(fxid, slot, 1, priority, 1.0f, 0.0f, 0.0f);
    }
    return AudPlayEffectVP(fxid, slot, 1, priority, 1.0f, (f32) gBumpLRTab[direction], (f32) gBumpFBTab[direction]);
}
