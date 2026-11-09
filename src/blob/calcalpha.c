#include "game/game.h"

f32 calcalpha(f32 *vector) {
    f32 value;
    f32 result;
    f32 z;
    value=vector[0];
    result=value;
    z=vector[2];
    if(z>0.0f) {
        if(z<400.0f) result=value/z;
        else result=value*calcalpha_far_scale_positive;
    } else if(z<0.0f) {
        if(z>-400.0f) result=(-value)/z;
        else result=value*calcalpha_far_scale_negative;
    }
    return result;
}
