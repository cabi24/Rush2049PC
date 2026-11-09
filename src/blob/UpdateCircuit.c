/* UpdateCircuit: a circuit after one of its races (MAT3 circuit.c). */
#include "game/game.h"

/* A player's circuit in progress (MAT3 tCircuit), in their profile's save data at 0x6F4. No pointers: the same
 * layout on both. */
struct tCircuit {
    u32 checksum;       /* 0x00 */
    u8 active;          /* 0x04 */
    u8 death;           /* 0x05 the death option: out of the circuit after a crash */
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
typedef struct Object { void *next; void *prev; void *fileh; u8 opaque_C[0x2C - 0xC]; Resource *resource; } Object;
typedef struct Handle { Object *object; } Handle;
typedef struct Player76 { u8 index, selector; u8 opaque[70]; Handle *handle; } Player76;
extern Player76 gPlayers[];

#define SWAP(a, b) { (a) ^= (b); (b) ^= (a); (a) ^= (b); }

/* Records the race just driven: each car's place (`values`, 3 bits each, the two players' swapped for the second
 * player's profile) into the circuit's ranks, `time` added to its time; the next race is set up (ContinueCircuit),
 * and the circuit recorded once done. A full circuit (type 3) gets harder after a good race (by the best player's
 * place and the leader's margin in points). The profile is saved. */
void UpdateCircuit(u8 *values, float time)
{
    u8 *data;
    struct tCircuit *circuit;
    int count, i, j, bit, best;
    int first, second, difference, level;

    data = gPlayers[gCircuitPlayer].handle->object->resource->data;
    circuit = (struct tCircuit *)(data + 1780);
    count = 6 - (gNumPlayers == 1 ? 0 : 2);
    if (gCircuitPlayer > 0) {
        SWAP(values[0], values[1]);
    }
    for (i = 0; i < count; i++) {
        bit = gCircuitTrack * 3;
        for (j = 0; j < 3; j++) {
            circuit->ranks[i * 9 + (bit >> 3)] =
                (circuit->ranks[i * 9 + (bit >> 3)] & ~(1 << (bit & 7))) |
                ((values[i] & 1) << (bit & 7));
            values[i] >>= 1;
            bit++;
        }
    }
    circuit->totalTime += time;
    circuit->track++;
    ContinueCircuit();
    gCircuitTrack--;
    if (circuit->track >= gCircuitSize) RecordCircuit();
    if (circuit->type == 3) {
        if (gNumPlayers == 1) best = gCircuitCars[0].totalPlace;
        else best = gCircuitCars[0].totalPlace < gCircuitCars[1].totalPlace ? gCircuitCars[0].totalPlace : gCircuitCars[1].totalPlace;
        for (i = 0; i < 6; i++) {
            if (gCircuitCars[i].totalPlace == 0) first = i;
            else if (gCircuitCars[i].totalPlace == 1) second = i;
        }
        difference = gCircuitCars[first].totalPoints - gCircuitCars[second].totalPoints;
        level = circuit->difficulty;
        if ((best == 0 && level < 5 && difference >= 20) ||
            (best == 0 && level < 4 && difference >= 6) ||
            (best < 2 && level < 3) ||
            (best < 3 && level < 2) ||
            (best < 4 && level <= 0)) circuit->difficulty = level + 1;
    }
    SavePlayerCircuit(gPlayers[gCircuitPlayer].handle);
}
