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
    u8 _pad18[0x4D - 0x18];
    u8 unk4D;
} InputRecord;
extern InputRecord gPlayers;

typedef struct {
    u8 pad0[8];
    s32 unk8;
} SavedPlayerSlot;
VIEW(SavedPlayerSlot **, gPlayers_save);
#define gPlayers_save gPlayers_save_view

typedef struct {
    u8 pad00[0x0C];
    u8 unk0C;
    u8 unk0D;
    u8 unk0E;
} CircuitOptions;
VIEW(CircuitOptions, gOptions);
#define gOptions gOptions_view

extern void *memset(void *s, s32 c, u32 n);

VIEW(s32, gSeconds);
#define gSeconds gSeconds_view

VIEW(s32[4], gCircuitDiff);
#define gCircuitDiff gCircuitDiff_view[0]

void PS_WriteFile(void **arg0, uintptr_t arg1, s32 arg2);

#define SLOT_DATA PROFILE_OFS(0x2C)  /* SaveSlot.data: the profile record's PC layout (pc_prelude.h) */
#define INPUT_SIZE 80  /* an input record (gPlayers): its pointer at +0x48 is 8 bytes (docs/port/POINTERS.md 10);
                       * the profile's save data, at SaveSlot.data, is read pointer-sized (intptr_t) */
#define DATA_PTR(x) (void *)(x)

void NewCircuit(s32 arg0) {
    s8 *options = (s8 *) &gOptions;   /* the game options, a byte each: [31] the circuit's death option */
    void *temp_a0;

    temp_a0 = DATA_PTR(*M2C_FIELD(**((SavedPlayerSlot ***) ((u8 *) &gPlayers_save + (gCircuitPlayer * INPUT_SIZE))), intptr_t **, SLOT_DATA) + 0x6F4);
    memset(temp_a0, 0, 0x4CU);
    PS_WriteFile(M2C_FIELD(**((SavedPlayerSlot ***) ((u8 *) &gPlayers_save + (gCircuitPlayer * INPUT_SIZE))), void ***, PROFILE_OFS(8)), (uintptr_t) temp_a0, 0x4C);
    M2C_FIELD(temp_a0, s8 *, 8) = (s8) arg0;
    M2C_FIELD(temp_a0, s32 *, 0xC) = (s32) (gSeconds & 0xFFFFF);
    M2C_FIELD(temp_a0, s8 *, 4) = 1;
    M2C_FIELD(temp_a0, f32 *, 0x10) = 0.0f;
    if (options[31] != 0) {
        M2C_FIELD(temp_a0, s8 *, 5) = 1;
    }
    M2C_FIELD(temp_a0, s8 *, 7) = (s8) (&gCircuitDiff)[arg0];
}

