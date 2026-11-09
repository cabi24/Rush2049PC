#include "game/game.h"

s32 AudioTimeout(f32 x,f32 delta) {f32 sum=x+delta;if(gAudioSeconds<x)sum-=14400.0f;return sum<gAudioSeconds;}
