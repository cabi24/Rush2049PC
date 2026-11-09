#include "game/game.h"

extern void *memset(void *s, s32 c, u32 n);

void PS_WriteFile(void **arg0, uintptr_t arg1, s32 arg2);

#define SLOT_DATA PROFILE_OFS(0x2C)  /* SaveSlot.data: the profile record's PC layout (pc_prelude.h) */

s32 ClearPlayerRecords(void **arg0) {
    intptr_t temp_s1;

    if (ReadPlayerData(arg0) == 0) {
        return 0;
    }
    temp_s1 = *M2C_FIELD(*arg0, intptr_t **, SLOT_DATA);
    memset((void *) (temp_s1 + 0x8C), 0, 0x480U);
    PS_WriteFile(M2C_FIELD(*arg0, void ***, PROFILE_OFS(8)), (uintptr_t) (temp_s1 + 0x8C), 0x480);
    memset((void *) (temp_s1 + 0x50C), 0, 0x100U);
    PS_WriteFile(M2C_FIELD(*arg0, void ***, PROFILE_OFS(8)), (uintptr_t) (temp_s1 + 0x50C), 0x100);
    memset((void *) (temp_s1 + 0x60C), 0, 0x60U);
    PS_WriteFile(M2C_FIELD(*arg0, void ***, PROFILE_OFS(8)), (uintptr_t) (temp_s1 + 0x60C), 0x60);
    memset((void *) (temp_s1 + 0x66C), 0, 0x18U);
    PS_WriteFile(M2C_FIELD(*arg0, void ***, PROFILE_OFS(8)), (uintptr_t) (temp_s1 + 0x66C), 0x18);
    memset((void *) (temp_s1 + 0x684), 0, 0x70U);
    PS_WriteFile(M2C_FIELD(*arg0, void ***, PROFILE_OFS(8)), (uintptr_t) (temp_s1 + 0x684), 0x70);
    memset((void *) (temp_s1 + 0x6F4), 0, 0x4CU);
    PS_WriteFile(M2C_FIELD(*arg0, void ***, PROFILE_OFS(8)), (uintptr_t) (temp_s1 + 0x6F4), 0x4C);
    if (PS_FlushFile(M2C_FIELD(*arg0, void ***, PROFILE_OFS(8)), 0) == 0) {
        PurgePlayerData(arg0);
        return 0;
    }
    RemovePlayerScores((intptr_t) arg0);
    PS_FlushPak((s32) M2C_FIELD(*M2C_FIELD(*arg0, void ***, PROFILE_OFS(8)), u8 *, PSF_OFS(0x10)), 1);
    return 1;
}

