#include "game/game.h"

typedef struct Resource { u8 *data; } Resource;
typedef struct Object { void *next; void *prev; void *fileh; u8 opaque_C[0x2C - 0xC]; Resource *resource; } Object;
typedef struct Handle { Object *object; } Handle;
typedef struct Player76 { u8 index,selector; u8 opaque[70]; Handle *handle; } Player76;
typedef struct Session76 {
    u8 prefix[5], option, enabled, level;
    s8 mode, length;
    u8 gap[2];
    unsigned int seed;
    float score;
    u8 packed[56];
} Session76;
typedef struct Ranking76 { u8 index,place; u16 score; u8 rest[72]; } Ranking76;
extern Player76 gPlayers[];
VIEW(Ranking76[], gCircuitCars);
#define gCircuitCars gCircuitCars_view

#define SWAP(a, b) { (a) ^= (b); (b) ^= (a); (a) ^= (b); }

void UpdateCircuit(u8 *values, float time)
{
    u8 *data;
    Session76 *session;
    int count, i, j, bit, best;
    int first, second, difference, level;

    data = gPlayers[gCircuitPlayer].handle->object->resource->data;
    session = (Session76 *)(data + 1780);
    count = 6 - (gNumPlayers == 1 ? 0 : 2);
    if (gCircuitPlayer > 0) {
        SWAP(values[0], values[1]);
    }
    for (i = 0; i < count; i++) {
        bit = gCircuitTrack * 3;
        for (j = 0; j < 3; j++) {
            session->packed[i * 9 + (bit >> 3)] =
                (session->packed[i * 9 + (bit >> 3)] & ~(1 << (bit & 7))) |
                ((values[i] & 1) << (bit & 7));
            values[i] >>= 1;
            bit++;
        }
    }
    session->score += time;
    session->length++;
    ContinueCircuit();
    gCircuitTrack--;
    if (session->length >= gCircuitSize) RecordCircuit();
    if (session->mode == 3) {
        if (gNumPlayers == 1) best = gCircuitCars[0].place;
        else best = gCircuitCars[0].place < gCircuitCars[1].place ? gCircuitCars[0].place : gCircuitCars[1].place;
        for (i = 0; i < 6; i++) {
            if (gCircuitCars[i].place == 0) first = i;
            else if (gCircuitCars[i].place == 1) second = i;
        }
        difference = gCircuitCars[first].score - gCircuitCars[second].score;
        level = session->level;
        if ((best == 0 && level < 5 && difference >= 20) ||
            (best == 0 && level < 4 && difference >= 6) ||
            (best < 2 && level < 3) ||
            (best < 3 && level < 2) ||
            (best < 4 && level <= 0)) session->level = level + 1;
    }
    SavePlayerCircuit(gPlayers[gCircuitPlayer].handle);
}
