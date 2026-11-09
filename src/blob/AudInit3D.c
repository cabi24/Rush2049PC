#include "game/game.h"

typedef struct Link {
    struct Link *next, *prev;
} Link;

typedef struct List {
    u8 indirect, doubly, pad[2];
    u32 count;
    Link *head, *tail;
} List;

typedef struct {
    /* 0x00 */ Link link;
    /* 0x08 */ s8 unk8;
    /* 0x09 */ s8 unk9;
    /* 0x0A */ u8 padA[0x32];
} RecA; /* 0x3C */

typedef struct RecB {
    /* 0x00 */ Link link;
    /* 0x08 */ s8 unk8;
    /* 0x09 */ s8 unk9;
    /* 0x0A */ u8 padA[0x22];
} RecB; /* 0x2C */

VIEW(List, gFreeListeners);
#define gFreeListeners gFreeListeners_view
VIEW(List, gActiveListeners);
#define gActiveListeners gActiveListeners_view
VIEW(List, gFreeEmitters);
#define gFreeEmitters gFreeEmitters_view
VIEW(List, gActiveEmitters);
#define gActiveEmitters gActiveEmitters_view
VIEW(RecA *, gListeners);
#define gListeners gListeners_view
VIEW(RecB *, gEmitters);
#define gEmitters gEmitters_view

#define N64A(x) PC_N64A(x)

void AudInit3D(s32 numA, s32 numB, f32 arg2, f32 arg3, s32 arg4) {
    s32 i;

    sound_list_init(&gFreeListeners, 0, 1);
    sound_list_init(&gActiveListeners, 0, 1);
    sound_list_init(&gFreeEmitters, 0, 1);
    sound_list_init(&gActiveEmitters, 0, 1);
    if (numA > 0 && numB > 0) {
        gNumListeners = numA;
        gListeners = AllocateMemoryHeap(0, gNumListeners * sizeof(RecA));
        PurgeableMemory(N64A(gListeners), 0);
        for (i = 0; i < gNumListeners; i++) {
            gListeners[i].unk8 = 1;
            gListeners[i].unk9 = 0;
            InsertLLE(&gFreeListeners, &gListeners[i].link, gFreeListeners.head);
        }
        gNumEmitters = numB;
        gEmitters = AllocateMemoryHeap(0, gNumEmitters * sizeof(RecB));
        PurgeableMemory(N64A(gEmitters), 0);
        for (i = 0; i < gNumEmitters; i++) {
            gEmitters[i].unk8 = 1;
            gEmitters[i].unk9 = 0;
            InsertLLE(&gFreeEmitters, &gEmitters[i].link, gFreeEmitters.head);
        }
        gSoundSpeed = 1100.0f * arg2;
        gFrontDistance = arg3;
        gCalcDoppler = arg4;
    }
}
