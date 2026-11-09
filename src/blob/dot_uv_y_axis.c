#include "game/game.h"

f32 dot_uv_y_axis(f32 *a,f32 *b) {
    return (a[0]*b[3]+a[1]*b[4])+a[2]*b[5];
}
