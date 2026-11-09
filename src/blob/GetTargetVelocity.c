/* GetTargetVelocity: the velocity of the target that moves a collision object (MAT3 targetpaths.c). */
#include "game/game.h"

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
        path = TARGET_PATH(state);
        if (collIdx == path->collIdx) {
            if (state->flags & TPATH_BACKWARD) {
                vel[0] = path->pts[state->ptOn].norm[0] * -state->curVel;
                vel[1] = path->pts[state->ptOn].norm[1] * -state->curVel;
                vel[2] = path->pts[state->ptOn].norm[2] * -state->curVel;
                return;
            }
            vel[0] = path->pts[state->ptOn].norm[0] * state->curVel;
            vel[1] = path->pts[state->ptOn].norm[1] * state->curVel;
            vel[2] = path->pts[state->ptOn].norm[2] * state->curVel;
            return;
        }
    }
}
