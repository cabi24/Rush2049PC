#include "game/game.h"

typedef struct Vec3 { f32 x, y, z; } Vec3;
typedef struct Packet Packet;
typedef struct PlayerRec {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 pad1[0x43];
    /* 0x44 */ s32 message;
    /* 0x48 */ void *ref;
} PlayerRec; /* 0x4C */
/* PC: an input record is 80 bytes; +0x44 stays 32 bits: a packet is a heap block, so its window address
 * (docs/port/POINTERS.md 10; pc_prelude.h's PC_N64A, docs/port/NOFIXED.md 2.3). */
#define PACKET(p) (s32)PC_N64A(p)

extern PlayerRec gPlayers[];
VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(Vec3, unitmatrix_origin);
#define unitmatrix_origin unitmatrix_origin_view
VIEW(f32[][3], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(s8[], gOptions);
#define gOptions gOptions_view

Packet *AudAddListener(Vec3 *a, Vec3 *b, Vec3 *c, Vec3 *d);

void StartListeners(void)
{
    s32 i;

    if (!(gGameState & 8)) {
        for (i = 0; i < gNumPlayers; i++) {
            if (gRace != 2 || gPlayers[i].unk0 == 0)
                gPlayers[i].message = PACKET(AudAddListener(&unitmatrix_origin, &unitmatrix_origin, (Vec3 *) unitmatrix[2], (Vec3 *) unitmatrix[1]));
            else gPlayers[i].message = -1;
        }
        AudSetMasterVol((f32)gOptions[13] / 10.0f, 0.1f, 0, 1);
    }
}
