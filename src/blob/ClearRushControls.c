#include "game/game.h"

typedef struct InputRecord {
    u8 pad00[1];
    u8 active;
    u8 pad02[2];
    s32 unk04;
    s32 unk08;
    s32 unk0C;
    f32 unk10;
    f32 unk14;
    u8 _pad18[0x48 - 0x18]; void *ref; u8 _pad18_4C[0x4D - 0x4C];
    u8 unk4D;
} InputRecord;
extern InputRecord gPlayers;

/* PC: an input record (gPlayers) is 80 bytes, its pointer at +0x48 is 8 (docs/port/POINTERS.md 10). */
#define INPUT_SIZE 80

void ClearRushControls(void) {
    s8 *var_v0;
    s32 var_v1;

    ClearControls();
    var_v0 = (s8 *) &gPlayers;
    var_v1 = 0;
    if (gNumPlayers > 0) {
        do {
            *(s32 *) (var_v0 + 8) = 0;
            *(s32 *) (var_v0 + 4) = 0;
            *(s32 *) (var_v0 + 0xC) = 0;
            *(f32 *) (var_v0 + 0x10) = 0.0f;
            *(f32 *) (var_v0 + 0x14) = 0.0f;
            var_v1 += 1;
            var_v0 += INPUT_SIZE;
        } while (var_v1 < gNumPlayers);
    }
}

