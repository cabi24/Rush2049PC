#include "game/game.h"

typedef struct Resource { u8 *data; } Resource;
typedef struct Object { void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC]; Resource *resource; } Object;
typedef struct Handle { Object *object; } Handle;
typedef struct Player76 { u8 index, selector; u8 pad2[70]; Handle *handle; } Player76;
typedef struct Session76 {
    u8 pad0[5], option, enabled, level, mode, length;
    u8 pad10[2];
    u32 seed;
} Session76;

extern Player76 gPlayers[];

VIEW(s16[][6], gTempCircuitPoints);
#define gTempCircuitPoints gTempCircuitPoints_view
VIEW(u32[], gNumCircuitTracks);
#define gNumCircuitTracks gNumCircuitTracks_view
VIEW(u32[6], gCircuitPoints);
#define gCircuitPoints gCircuitPoints_view

s32 SetCircuitCode(u8 *input)
{
    u32 mode, option, length, seed, level;
    u32 values[6];
    u32 total_weights, total_values;
    u32 ranks[6];
    s32 i, j;
    Session76 *session;

    session = (Session76 *)(gPlayers[gCircuitPlayer].handle->object->resource->data + 1780);
    NewPassword(86, 14);
    if (!SetPasswordString(input)) return 0;
    GetPasswordElement(&mode, 3);
    GetPasswordElement(&option, 1);
    GetPasswordElement(&length, 5);
    GetPasswordElement(&seed, 20);
    GetPasswordElement(&level, 3);
    for (i = 0; i < 6; i++) GetPasswordElement(&values[i], 9);
    if (length == 0 || length >= gNumCircuitTracks[mode] * 4) return 0;
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
    if (gCircuitPoints[0] * length < values[ranks[0]]) return 0;
    for (total_weights = 0, i = 0; i < 6; i++) total_weights += gCircuitPoints[i];
    total_values = 0;
    for (i = 0; i < 6; i++) total_values += values[ranks[i]];
    if (total_values != total_weights * length) return 0;
    NewCircuit(mode);
    session->enabled = 1;
    session->option = option;
    session->length = length;
    gTempCircuitFirstTrack[gPlayers[gCircuitPlayer].selector] = length;
    session->seed = seed;
    session->level = level;
    for (i = 0; i < 6; i++) gTempCircuitPoints[gPlayers[gCircuitPlayer].selector][i] = values[i];
    ContinueCircuit();
    return 1;
}
