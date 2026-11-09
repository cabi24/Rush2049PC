#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(s8, view3_switch);
#define view3_switch view3_switch_view

#define rear_y (rear_y[0])

#define rear_z (rear_z[0])

void init_view3(void *arg0) {
    s8 temp_v0;

    M2C_FIELD(arg0, s8 *, CAR_OFS(0x35E)) = 2;
    temp_v0 = M2C_FIELD(arg0, s8 *, CAR_OFS(0x35C));
    M2C_FIELD(arg0, s8 *, CAR_OFS(0x35D)) = (s8) M2C_FIELD(arg0, s8 *, CAR_OFS(0x35E));
    if (gGameState & 8) {
        (&rear_y)[temp_v0] = 5.0f;
        (&rear_z)[temp_v0] = 20.0f;
    } else {
        (&rear_y)[temp_v0] = 3.0f;
        (&rear_z)[temp_v0] = 16.0f;
    }
    (&view3_switch)[temp_v0] = 1;
}
