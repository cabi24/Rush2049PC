/* calcalpha.c: a tire's slip angle (MAT3 tires.c, calcalpha()).
 *
 * dotireforce's helper for the side force. */
#include "game/game.h"

/* The slip angle, as its tangent, of a tire moving at tirev (the tire's frame: x sideways, z rolling forward, feet a
 * second): sideways over forward speed. From 400 ft/s of forward speed either way (and at a standstill) it uses a
 * fixed scale instead (calcalpha_far_scale_positive, _negative: MAT3 0.0025 = 1/400 both) so the angle can't blow up
 * as the forward speed nears 0 or grow without bound. */
f32 calcalpha(f32 *tirev) {
    f32 side;
    f32 alpha;
    f32 forward;
    side=tirev[0];
    alpha=side;
    forward=tirev[2];
    if(forward>0.0f) {
        if(forward<400.0f) alpha=side/forward;
        else alpha=side*calcalpha_far_scale_positive;
    } else if(forward<0.0f) {
        if(forward>-400.0f) alpha=(-side)/forward;
        else alpha=side*calcalpha_far_scale_negative;
    }
    return alpha;
}
