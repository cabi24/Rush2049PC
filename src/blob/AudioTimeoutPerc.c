#include "game/game.h"

f32 AudioTimeoutPerc(f32 arg0, f32 arg1) {
    f32 var_f2;

    var_f2 = gAudioSeconds;
    if (gAudioSeconds < arg0) {
        var_f2 = gAudioSeconds + 14400.0f;
    }
    return (var_f2 - arg0) / arg1;
}
