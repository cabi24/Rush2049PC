#include "game/game.h"

typedef struct {
    s32 handle;
    f32 unk4[4];
} SndRec; /* 0x14 */

typedef struct MODELDAT {
    u8 pad0[0x7C6];
    s16 slot;
    u8 pad7C8[4];
    s8 unk7CC;
} MODELDAT;

VIEW(SndRec[][3], gRoadNoises);
#define gRoadNoises gRoadNoises_view

void StopCarSounds(MODELDAT *m) {
    s32 slot = m->slot;
    s32 i;

    if (PC_CAR_SOUNDS_ON(slot)) {
        PC_CAR_SOUNDS_ON(slot) = 0;
        PC_ENGINE_PHASE(slot) = (slot & 1) == 0;
        for (i = 0; i < 2; i++) {
            if (i) {}
            if (PC_ENGINE_SOUND_HANDLE(slot, i) != -1) {
                StopSound(PC_ENGINE_SOUND_REC(slot, i), m->unk7CC != 2);
            }
        }
        if (PC_ENGINE_SOUND_HANDLE(slot, 3) != -1) {
            StopSound(PC_ENGINE_SOUND_REC(slot, 3), m->unk7CC != 2);
        }
        StopSound(PC_SCREECH(slot), m->unk7CC != 2);
        if (m->unk7CC == 2) {
            StopSound(&gRoadNoises[slot][0], 0);
            StopSound(&gRoadNoises[slot][1], 0);
            StopSound(&gRoadNoises[slot][2], 0);
            AudStopEffect(gScrapes[slot]);
            gScrapes[slot] = -1;
            gScrapeState[slot] = 0;
            gScrapeTime[slot] = 0.0f;
            AudStopEffect(gHornSounds[slot]);
            gHornSounds[slot] = -1;
            AudStopEffect(gBogSounds[slot]);
            gBogSounds[slot] = -1;
        }
    }
}
