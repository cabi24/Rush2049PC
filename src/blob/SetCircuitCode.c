/* SetCircuitCode: a circuit continued from its code (MAT3 circuit.c). */
#include "game/game.h"

/* A player's circuit in progress (MAT3 tCircuit), in their profile's save data at 0x6F4. No pointers: the same
 * layout on both. */
struct tCircuit {
    u32 checksum;       /* 0x00 */
    u8 active;          /* 0x04 */
    u8 death;           /* 0x05 the death death: out of the circuit after a crash */
    u8 temp;            /* 0x06 entered by a circuit code: the points so far are gTempCircuitPoints' */
    u8 difficulty;      /* 0x07 */
    s8 type;            /* 0x08 kCircuitBeginner ... kCircuitExtreme */
    s8 track;           /* 0x09 the races driven */
    u8 pad0A[2];
    u32 seed;           /* 0x0C the circuit's random seed (its tracks, its drones) */
    f32 totalTime;      /* 0x10 */
    u8 ranks[54];       /* 0x14 each car's place in each race, 3 bits a race, 9 bytes a car (MAT3 [6][9]) */
};

/* A player's profile (SaveSlot, host layout: pc_prelude.h), its save data, and its input record (gPlayers). */
typedef struct Resource { u8 *data; } Resource;
typedef struct Object { void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC]; Resource *resource; } Object;
typedef struct Handle { Object *object; } Handle;
typedef struct Player76 { u8 index, selector; u8 pad2[70]; Handle *handle; } Player76;

extern Player76 gPlayers[];

/* Reads circuit code `input` (a password of 86 bits in 14 characters: the type, the death death, the races driven,
 * the seed, the difficulty, the six cars' points) and, if it holds together (races within the circuit, the points
 * those races can give, the best no more than all wins), starts that circuit for the player (NewCircuit) at that
 * race with those points (gTempCircuitPoints) and sets it up (ContinueCircuit). Returns 1, or 0 for a bad code. */
s32 SetCircuitCode(u8 *input)
{
    u32 type, death, track, seed, difficulty;
    u32 values[6];
    u32 total_weights, total_values;
    u32 ranks[6];
    s32 i, j;
    struct tCircuit *circuit;

    circuit = (struct tCircuit *)(gPlayers[gCircuitPlayer].handle->object->resource->data + 1780);
    NewPassword(86, 14);
    if (!SetPasswordString(input)) return 0;
    GetPasswordElement(&type, 3);
    GetPasswordElement(&death, 1);
    GetPasswordElement(&track, 5);
    GetPasswordElement(&seed, 20);
    GetPasswordElement(&difficulty, 3);
    for (i = 0; i < 6; i++) GetPasswordElement(&values[i], 9);
    if (track == 0 || track >= gNumCircuitTracks[type] * 4) return 0;
    for (i = 0; i < 6; i++) ranks[i] = i;
    for (i = 0; i < 5; i++) {
        for (j = i + 1; j < 6; j++) {
            if (values[ranks[i]] < values[ranks[j]]) {
                ranks[i] ^= ranks[j];
                ranks[j] ^= ranks[i];
                ranks[i] ^= ranks[j];
            }
        }
    }
    if (gCircuitPoints[0] * track < values[ranks[0]]) return 0;
    for (total_weights = 0, i = 0; i < 6; i++) total_weights += gCircuitPoints[i];
    total_values = 0;
    for (i = 0; i < 6; i++) total_values += values[ranks[i]];
    if (total_values != total_weights * track) return 0;
    NewCircuit(type);
    circuit->temp = 1;
    circuit->death = death;
    circuit->track = track;
    gTempCircuitFirstTrack[gPlayers[gCircuitPlayer].selector] = track;
    circuit->seed = seed;
    circuit->difficulty = difficulty;
    for (i = 0; i < 6; i++) gTempCircuitPoints[gPlayers[gCircuitPlayer].selector][i] = values[i];
    ContinueCircuit();
    return 1;
}
