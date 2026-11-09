/* AnimateKnockdown: the animation of the signs and meters a car knocks down (MAT3 targets.c _AnimateKnockdown). It
 * is the environment visual StartKnockdown sets up: each frame it tips the target over by its spin rate, for the
 * frame count StartKnockdown gave it, then lets the visual go. */
#include "game/game.h"

/* One frame of the fall: turn the target by its spin (its data: f32[3] radians a frame about x, y, z) and count a
 * frame off its frames; when none are left, release the visual. Nothing moves while the game is paused. op 0
 * releases the visual. */
void AnimateKnockdown(struct pc_visual *v, s16 op)
{
    tTarget *t;
    void *spin;
    s16 framesLeft;

    /* Original game: op 0 releases the visual but doesn't return (MAT3 the same), so an unpaused release goes on to
     * animate the target, and releases the visual a second time if the fall is over. */
    if (op == 0) {
        ReleaseEnviron(v, 1);
    }
    if (gPauseState == 0) {
        t = (tTarget *) v->data;
        spin = t->data;
        framesLeft = t->frames;
        if (framesLeft != 0) {
            t->frames = (s16) (framesLeft - 1);
            RotateUV(spin, t->mat.uvs);
            framesLeft = t->frames;
        }
        if (framesLeft == 0) {
            ReleaseEnviron(v, 1);
        }
    }
}
