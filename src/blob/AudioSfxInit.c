#include "game/game.h"

typedef struct Link {
    struct Link *next, *prev;
} Link;

typedef struct List {
    u8 indirect, doubly, pad[2];
    u32 count;
    Link *head, *tail;
} List;

typedef struct { u8 bytes[24]; } Rec24;
typedef struct Rec32 { Link link; u8 kind, busy, pad[2]; void *entry; f32 values[4]; } Rec32;

VIEW(List, gCmdQ);
#define gCmdQ gCmdQ_view
VIEW(List, gFreeQ);
#define gFreeQ gFreeQ_view

VIEW(Rec24 *, gSfxInstances);
#define gSfxInstances gSfxInstances_view
VIEW(Rec32 *, audiosfx_gCmds);
#define audiosfx_gCmds audiosfx_gCmds_view

#define N64A(x) PC_N64A(x)
void *memset(void *, s32, u32);

void AudioSfxInit(s32 count) {
    s32 i;
    s32 n;

    sound_list_init(&gCmdQ, 0, 1);
    if (count == 0) {
    }
    gNumInstances = count;
    sound_list_init(&gFreeQ, 0, 1);
    gSfxInstances = AllocateMemoryHeap(0, gNumInstances * sizeof(Rec24));
    PurgeableMemory(N64A(gSfxInstances), 0);
    memset(gSfxInstances, 0, gNumInstances * sizeof(Rec24));
    n = count * 3;
    audiosfx_gCmds = AllocateMemoryHeap(0, n * sizeof(Rec32));
    PurgeableMemory(N64A(audiosfx_gCmds), 0);
    memset(audiosfx_gCmds, 0, n * sizeof(Rec32));
    for (i = 0; i < n; i++) {
        InsertLLE(&gFreeQ, &audiosfx_gCmds[i].link, gFreeQ.head);
    }
    gNumChannels = count;
    for (i = 0; i < 5; i++) {
        gStoppedChannels[i] = 0;
    }
    gStopAllEffects = 0;
    audiosfx_gInited = 1;
}
