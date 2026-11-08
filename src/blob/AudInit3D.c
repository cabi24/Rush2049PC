/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Not a HUD routine (historical label): initialises four object lists (gFreeListeners, gActiveListeners,
 * gFreeEmitters, gActiveEmitters; List = { u8 indirect, doubly; u32 count; Link *head, *tail }) as empty
 * doubly-linked direct lists, and when both counts are positive allocates two record pools
 * (numA * 60 bytes -> gListeners, numB * 44 bytes -> gEmitters), marks every record
 * (+8 = 1, +9 = 0) and appends it to list gFreeListeners / gFreeEmitters with InsertLLE(list, rec, list->head).
 * Finally gSoundSpeed = 1100.0f * arg2, gFrontDistance = arg3, gCalcDoppler = arg4.
 *
 * Shape: the four list initialisations are an inlined helper `list->indirect = a; list->doubly = b;
 * ClearLL(list);` where ClearLL (head = tail = count = 0, locked) is inlined in turn; the
 * store order (+1, +0, +8, +0xC, +4) and the `li a2,1` argument web prove it. It is NOT InitLL,
 * whose own store order is +0, +1, +4, +0xC, +8. Record bytes +8/+9 are signed (s8): with u8 the
 * stored 1 shares the helper's argument web. The helper is written `static` here; no retail symbol
 * is known for it. ClearLL is defined in this file only so the single-file scorer can inline it:
 * in the whole-program unit use the prototype (the whole-program unit: EQUAL either way).
 * Own rodata: 1100.0f (retail 0x44898000 at 0x80123F90, checked).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

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

typedef struct {
    /* 0x00 */ Link link;
    /* 0x08 */ s8 unk8;
    /* 0x09 */ s8 unk9;
    /* 0x0A */ u8 padA[0x22];
} RecB; /* 0x2C */

extern List gFreeListeners;
extern List gActiveListeners;
extern List gFreeEmitters;
extern List gActiveEmitters;
extern RecA *gListeners;
extern RecB *gEmitters;
extern s32 gNumListeners;
extern s32 gNumEmitters;
extern f32 gSoundSpeed;
extern f32 gFrontDistance;
extern u8 gCalcDoppler;

void *AllocateMemoryHeap(s32 arg0, s32 arg1);
/* PurgeableMemory's definition takes the block's 32-bit N64 address: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
void PurgeableMemory(u32 arg0, s32 arg1);
#define N64A(x) PC_N64A(x)
void InsertLLE(List *list, Link *object, Link *before);

void ClearLL(List *list);


void sound_list_init(List *list, s8 indirect, s8 doubly);
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
