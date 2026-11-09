#include "game/game.h"

typedef struct { u8 pad[1860]; s8 tab[1]; } Inner;
typedef struct { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; Inner **inner; } Mid;
typedef struct Outer { Mid *mid; } Outer;
s8 GetPlayerCarSetting(Outer *a0, u8 a1, u8 a2) {
    return (*a0->mid->inner)->tab[a1 * 16 + a2];
}
