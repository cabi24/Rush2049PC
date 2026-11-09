/* GetTargetVelocity: the velocity of the target that moves a collision object (MAT3 targetpaths.c). */
#include "game/game.h"

/* A target path (MAT3 tBasicTargetPath, a PTHD record; targetpaths/group.c has it all): the fields this reads. N64
 * layout: pts is a 32-bit window address. */
typedef struct tBasicTargetPath {
    u8 pad00[0x10];
    s32 flags;      /* 0x10 */
    s16 numPts;     /* 0x14 */
    s16 collIdx;    /* 0x16 its collision object */
    u32 trigPtr;    /* 0x18 */
    u32 pts;        /* 0x1C its points (TargetPathPt) */
} tBasicTargetPath;

extern tTarget *CollisionTargetPtrs[];

/* Sets `vel` to the velocity of the path target whose collision object is `collIdx` (registered by
 * StartPathTarget): its speed along its stretch's direction, backwards if it goes backwards; zero if no target
 * moves that object. A car on a moving surface moves with it (MovingSurfaceForce). */
void GetTargetVelocity(s32 collIdx, f32 *vel) {
    s32 i;
    tTargetStructPath *state;
    tBasicTargetPath *path;

    vel[0] = 0.0f;
    vel[1] = 0.0f;
    vel[2] = 0.0f;
    for (i = 0; i < numCollisionTargetPtrs; i++) {
        state = CollisionTargetPtrs[i]->data;
        path = (tBasicTargetPath *) PC_N64P(state->pathData);
        if (collIdx == path->collIdx) {
            if (state->flags & TPATH_BACKWARD) {
                vel[0] = ((TargetPathPt *) PC_N64P(path->pts))[state->ptOn].norm[0] * -state->curVel;
                vel[1] = ((TargetPathPt *) PC_N64P(path->pts))[state->ptOn].norm[1] * -state->curVel;
                vel[2] = ((TargetPathPt *) PC_N64P(path->pts))[state->ptOn].norm[2] * -state->curVel;
                return;
            }
            vel[0] = ((TargetPathPt *) PC_N64P(path->pts))[state->ptOn].norm[0] * state->curVel;
            vel[1] = ((TargetPathPt *) PC_N64P(path->pts))[state->ptOn].norm[1] * state->curVel;
            vel[2] = ((TargetPathPt *) PC_N64P(path->pts))[state->ptOn].norm[2] * state->curVel;
            return;
        }
    }
}
