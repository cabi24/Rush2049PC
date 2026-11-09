/* GetCircuitCode: the code that continues a player's circuit later (MAT3 circuit.c). */
#include "game/game.h"

/* A player's circuit in progress (MAT3 tCircuit), in their profile's save data at 0x6F4. */
struct tCircuit {
    u32 checksum;       /* 0x00 */
    u8 active;          /* 0x04 */
    u8 death;           /* 0x05 */
    u8 temp;            /* 0x06 */
    u8 difficulty;      /* 0x07 */
    s8 type;            /* 0x08 */
    s8 track;           /* 0x09 */
    u8 pad0A[2];
    u32 seed;           /* 0x0C */
};

/* A player's profile (SaveSlot): its save data, read pointer-sized (intptr_t: the profile's PC layout). */
typedef struct
{
    u8 pad0[8];
    s32 unk8;
} SavedPlayerSlot;
VIEW(SavedPlayerSlot **, gPlayers_save);
#define gPlayers_save gPlayers_save_view

void GetPasswordString(intptr_t code);

/* PC: an input record (gPlayers) is 80 bytes, its pointer at +0x48 is 8 (docs/port/POINTERS.md 10), and
 * SaveSlot.data is at the profile record's PC offset (pc_prelude.h, POINTERS.md 9.10). */
#define INPUT_SIZE 80
#define SLOT_DATA PROFILE_OFS(0x2C)

/* Writes the circuit player's circuit code into `code`: the password (86 bits in 14 characters) of the circuit's
 * type, death option, races driven (the race in progress not counted: gCircuitTrack is set back), seed and
 * difficulty, and the six cars' points (SetCircuitCode reads it). */
void GetCircuitCode(intptr_t code)
{
    s32 i;
    struct tCircuit *circuit;
    circuit = (struct tCircuit *) ((*(*((intptr_t **) ((**((s8 ***) (((s8 *) (&gPlayers_save)) + (gCircuitPlayer * INPUT_SIZE)))) + SLOT_DATA)))) + 0x6F4);
    ContinueCircuit();
    gCircuitTrack -= 1;
    NewPassword(0x56U, 0xEU);
    StorePasswordElement((u32) circuit->type, 3U);
    StorePasswordElement((u32) circuit->death, 1U);
    StorePasswordElement((u32) circuit->track, 5U);
    StorePasswordElement(circuit->seed, 20U);
    StorePasswordElement((u32) circuit->difficulty, 3U);
    for (i = 0; i < 6; i++) {
        StorePasswordElement((u32) gCircuitCars[i].totalPoints, 9U);
    }
    GetPasswordString(code);
}
