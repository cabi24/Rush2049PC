/* AnimateKnockdown: the animation of the signs and meters a car knocks down (MAT3 targets.c _AnimateKnockdown). It
 * is the environment visual StartKnockdown sets up: each frame it tips the target over by its spin rate, for the
 * frame count StartKnockdown gave it, then lets the visual go. */
#include "game/game.h"

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
typedef struct Target {
    u8 pad00[0x14];
    f32 uvs[3][3];          /* 0x14 mat: its orientation */
    u8 pad38[0x5A - 0x38];
    s16 frames;             /* 0x5A the frames of the fall left */
    u8 pad5C[0x6C - 0x5C];
    u32 data;               /* 0x6C its spin a frame, f32[3] radians about x, y, z (a window address) */
} Target;

/* One frame of the fall: turn the target by its spin and count a frame off; when none are left, release the
 * visual. Nothing moves while the game is paused. op 0 releases the visual. */
void AnimateKnockdown(struct pc_visual *v, s16 op)
{
    Target *t;
    void *spin;
    s16 framesLeft;

    /* Original game: op 0 releases the visual but doesn't return (MAT3 the same), so an unpaused release goes on to
     * animate the target, and releases the visual a second time if the fall is over. */
    if (op == 0) {
        ReleaseEnviron(v, 1);
    }
    if (gPauseState == 0) {
        t = (void *) PC_N64P(v->data);
        /* the spin goes to RotateUV zero-extended, as the pointer it is */
        spin = (void *) PC_N64P(t->data);
        framesLeft = t->frames;
        if (framesLeft != 0) {
            t->frames = (s16) (framesLeft - 1);
            RotateUV(spin, t->uvs);
            framesLeft = t->frames;
        }
        if (framesLeft == 0) {
            ReleaseEnviron(v, 1);
        }
    }
}
