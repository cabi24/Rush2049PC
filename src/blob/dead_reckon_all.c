#include "game/game.h"

typedef struct ModelRecord {
    u8 pad000[0x7C6];
    u16 unk7C6;
    u8 pad7C8[0x7E8 - 0x7C8];
    u8 unk7E8;
} ModelRecord;

/* GameCar's pad0EC[x] is the car's byte 0xEC + x; on the PC it is at CAR_OFS(0xEC + x) (docs/port/CARS.md). */
#define PAD0EC(a, x) (*((u8 *)(a) - 0xEC + CAR_OFS(0xEC + (x))))
void dead_reckon_all(void) {
    ModelRecord *r;
    s32 i;
    for (i = 0, r = PC_MODELS; i != PC_RACE_CARS; i++, r = (ModelRecord *) ((s8 *) r + 0x808)) {
        if ((*(s16 *) ((s8 *) r + 0x7C8) != 0) && ((s8) PC_CAR_STATE(i) < 2)) {
            dead_reckon_posuv(r);
        }
    }
}
