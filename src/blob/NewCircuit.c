/* NewCircuit: a player starts a circuit (MAT3 circuit.c). */
#include "game/game.h"

/* A player's circuit in progress (MAT3 tCircuit), in their profile's save data at 0x6F4. */
struct tCircuit {
    u32 checksum;       /* 0x00 */
    u8 active;          /* 0x04 */
    u8 death;           /* 0x05 the death option */
    u8 temp;            /* 0x06 */
    u8 difficulty;      /* 0x07 */
    s8 type;            /* 0x08 */
    s8 track;           /* 0x09 */
    u8 pad0A[2];
    u32 seed;           /* 0x0C */
    f32 totalTime;      /* 0x10 */
    u8 ranks[6][9];     /* 0x14 */
};

/* A player's profile (SaveSlot, host layout): what this reads through its offsets. */
typedef struct {
    u8 pad0[8];
    s32 unk8;
} SavedPlayerSlot;


extern void *memset(void *s, s32 c, u32 n);

VIEW(s32[4], gCircuitDiff);
#define gCircuitDiff gCircuitDiff_view[0]

void PS_WriteFile(void **fileh, uintptr_t data, s32 size);

#define SLOT_DATA PROFILE_OFS(0x2C)  /* SaveSlot.data: the profile record's PC layout (pc_prelude.h) */
#define INPUT_SIZE 80  /* an input record (gPlayers): its pointer at +0x48 is 8 bytes (docs/port/POINTERS.md 10);
                       * the profile's save data, at SaveSlot.data, is read pointer-sized (intptr_t) */
#define DATA_PTR(x) (void *)(x)

/* Starts a circuit of type `type` for the circuit player: their circuit record cleared and written to their pak
 * file (PS_WriteFile), active, its seed from the clock, the death option from the options, its difficulty the
 * type's. */
void NewCircuit(s32 type) {
    struct tCircuit *circuit;

    circuit = DATA_PTR(*M2C_FIELD(**((SavedPlayerSlot ***) ((u8 *) &gPlayers_save + (gCircuitPlayer * INPUT_SIZE))), intptr_t **, SLOT_DATA) + 0x6F4);
    memset(circuit, 0, sizeof(struct tCircuit));
    PS_WriteFile(M2C_FIELD(**((SavedPlayerSlot ***) ((u8 *) &gPlayers_save + (gCircuitPlayer * INPUT_SIZE))), void ***, PROFILE_OFS(8)), (uintptr_t) circuit, sizeof(struct tCircuit));
    circuit->type = (s8) type;
    circuit->seed = *(s32 *) &gSeconds & 0xFFFFF;   /* the game time's float bits (MAT3: *(unsigned int *)&gSeconds) */
    circuit->active = 1;
    circuit->totalTime = 0.0f;
    if (gOptions[kOptDeathMode] != 0) {
        circuit->death = 1;
    }
    circuit->difficulty = (s8) (&gCircuitDiff)[type];
}
