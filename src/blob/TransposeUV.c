#include "game/game.h"

void TransposeUV(f32 *a) {f32 t;t=a[1];a[1]=a[3];a[3]=t;t=a[2];a[2]=a[6];a[6]=t;t=a[5];a[5]=a[7];a[7]=t;}
