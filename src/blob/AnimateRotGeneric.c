/* AnimateRotGeneric: the animation of the track objects that spin in place forever, such as the windmills' rotors
 * and a trolley's wheel (MAT3 targets.c _AnimateRotGeneric). It is the environment visual's function, called each
 * frame with op 1, and with op 0 when the visual is released. */
#include "game/game.h"

/* Spin the visual's target by its rate (the target's data: its kind's record, f32[3] radians a second about x, y,
 * z: MAT3 tTargetStructRot) times the frame's length; nothing moves while the game is paused. With op 0 the visual
 * goes back to the free list. */
void AnimateRotGeneric(struct pc_visual *v, s16 op)
{
    tTarget *t;
    f32 *rate;
    f32 angle[3];

    if (op == 0) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState == 0) {
        t = (tTarget *) v->data;
        rate = t->data;
        angle[0] = rate[0] * gFrameSeconds;
        angle[1] = rate[1] * gFrameSeconds;
        angle[2] = rate[2] * gFrameSeconds;
        RotateUV(angle, t->mat.uvs);
    }
}
