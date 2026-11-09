/* AnimateRotGeneric: the animation of the track objects that spin in place forever, such as the windmills' rotors
 * and a trolley's wheel (MAT3 targets.c _AnimateRotGeneric). It is the environment visual's function, called each
 * frame with op 1, and with op 0 when the visual is released. */
#include "game/game.h"

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
typedef struct Target {
    u8 pad00[0x14];
    f32 uvs[3][3];          /* 0x14 mat: its orientation */
    u8 pad38[0x6C - 0x38];
    u32 data;               /* 0x6C its spin rate, f32[3] radians a second about x, y, z (a window address) */
} Target;

/* Spin the visual's target by its rate times the frame's length; nothing moves while the game is paused. With op
 * 0 the visual goes back to the free list. */
void AnimateRotGeneric(struct pc_visual *v, s16 op)
{
    Target *t;
    f32 *rate;
    f32 angle[3];

    if (op == 0) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState == 0) {
        t = (Target *) v->data;
        rate = (void *) PC_N64P(t->data);
        angle[0] = rate[0] * gFrameSeconds;
        angle[1] = rate[1] * gFrameSeconds;
        angle[2] = rate[2] * gFrameSeconds;
        RotateUV(angle, t->uvs);
    }
}
