#include "game/game.h"

typedef struct Obj { char pad[1032]; float f; char pad2[964]; s16 out; } Obj;

void mcommunication(Obj *a0) {
    a0->out = a0->f * rad_per_sec_to_rpm * mcommunication_rpm_scale;
}
