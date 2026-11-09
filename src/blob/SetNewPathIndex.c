#include "game/game.h"

/* GameCar's pad0EC[x] is the car's byte 0xEC + x; on the PC it is at CAR_OFS(0xEC + x) (docs/port/CARS.md). */
#define PAD0EC(a, x) (*((u8 *)(a) - 0xEC + CAR_OFS(0xEC + (x))))
void SetNewPathIndex(void *arg0, s16 arg1) {
 u8 *temp_v0;
 temp_v0 = &PC_CAR_NAV(M2C_FIELD(arg0, s16 *, 0x7C6));
M2C_FIELD(temp_v0, s16 *, 0x24) = arg1;
M2C_FIELD(temp_v0, f32 *, 0x20) = (f32) gModelTime;
M2C_FIELD(temp_v0, s16 *, 0x26) = -1;
M2C_FIELD(temp_v0, f32 *, 0) = 20.0f;
M2C_FIELD(temp_v0, f32 *, 4) = 0.0f;
}
