#include "game/game.h"

typedef struct { u8 pad[71]; u8 mode; u8 pad2[5]; u8 b77; u8 pad3[19]; u8 b97; u8 pad4[15]; u8 b113; u8 pad5[19]; u8 b133; } Inner;
typedef struct { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; Inner **inner; } Mid;
typedef struct Outer { Mid *mid; } Outer;
u8 GetPlayerCar(Outer *a0) {
    Inner *v = *a0->mid->inner;
    switch (v->mode) {
    case 6: return v->b97;
    case 4: return v->b113;
    case 5: return v->b133;
    default: return v->b77;
    }
}
