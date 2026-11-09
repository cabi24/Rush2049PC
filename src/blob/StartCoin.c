#include "game/game.h"

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

VIEW(M2C_UNK, targetInfo);
#define targetInfo targetInfo_view
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

/* PC: the target arg0 (a level-object node) keeps its N64 layout: its state (+0x6C) is a 32-bit window address
 * (docs/port/POINTERS.md 10). */
#define NODE_STATE(t) (void *) PC_N64P(M2C_FIELD(t, u32 *, 0x6C))
void StartCoin(void *arg0) {
    s8 temp_a2;
    void *temp_s1;

    temp_a2 = M2C_FIELD(arg0, s8 *, 0x5C);
    M2C_FIELD(arg0, u8 *, 4) = (u8) (M2C_FIELD(arg0, u8 *, 4) & 0xFFFB);
    temp_s1 = NODE_STATE(arg0);
    if ((PC_CAR_VIEW(temp_a2) >= 0) && (IsCoinFound(temp_a2, M2C_FIELD(temp_s1, u16 *, 0x4A)) == 0)) {
        FoundCoin(M2C_FIELD(arg0, s8 *, 0x5C), M2C_FIELD(temp_s1, u16 *, 0x4A));
        MBOX_HideObject(M2C_FIELD(arg0, s32 *, 0xC), 1, 1 << PC_CAR_VIEW(M2C_FIELD(arg0, s8 *, 0x5C)));
        MBOX_HideObject(M2C_FIELD(temp_s1, s32 *, 0), 1, 1 << PC_CAR_VIEW(M2C_FIELD(arg0, s8 *, 0x5C)));
        if ((M2C_FIELD(temp_s1, u16 *, 0x48) &= ~(1 << PC_CAR_VIEW(M2C_FIELD(arg0, s8 *, 0x5C)))) == 0) {
            M2C_FIELD(arg0, u8 *, 4) = (u8) (M2C_FIELD(arg0, u8 *, 4) | 0x80);
        }
        /* the target type's start sound, targetInfo[type].startSnd (N64 +0x1C) */
        PlayTargetSound(*(s32*)((u8 *) &targetInfo + (M2C_FIELD(arg0, s16 *, 0x10) * TINFO_OFS(0x30)) + TINFO_OFS(0x1C)), M2C_FIELD(arg0, s8 *, 0x5C), (u8 *) arg0 + 0x38, 2);
    }
}

