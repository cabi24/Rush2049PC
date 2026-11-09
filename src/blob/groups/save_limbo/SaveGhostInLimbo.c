#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct Header88 {
    /* 0x00 */ u8 pad0[6];
    /* 0x06 */ s8 player;
    /* 0x07 */ u8 mode;
    /* 0x08 */ u8 pad8[14];
    /* 0x16 */ u8 name[13];
    /* 0x23 */ u8 pad23[5];
    /* 0x28 */ u8 ident[8];
    /* 0x30 */ u8 pad30[8];
    /* 0x38 */ f32 score;
    /* 0x3C */ u32 bytes;
    /* 0x40 */ u8 pad40[12];
    /* 0x4C */ u32 offset;
    /* 0x50 */ u32 head;
    /* 0x54 */ u32 tail;
} Header88; /* 0x58 */

typedef struct Record44 {
    /* 0x00 */ void *ll; void **fileh;   /* a ghost, 64 bytes on the PC (docs/port/POINTERS.md 9.10) */
    /* 0x08 */ s8 player;
    /* 0x09 */ u8 mode;
    /* 0x0A */ u8 name[13];
    /* 0x17 */ u8 pad17;
    /* 0x18 */ u8 ident[8];
    /* 0x20 */ f32 score;
    /* 0x24 */ void *unk24;
    /* 0x28 */ Header88 **handle;
} Record44; /* 0x2C */


typedef struct OSMesgQueue OSMesgQueue;

VIEW(Record44 **, gLimboGhosts);
#define gLimboGhosts gLimboGhosts_view

VIEW(s32, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

void *memcpy(void *, const void *, u32);

static void release_handle(u32 address);

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void SaveGhostInLimbo(Header88 *entry, void *payload) {
    s32 i;
    Header88 **h;
    s32 j;
    s32 k;
    Record44 **row;

    if (expansion_pak_present == 0) {
        row = gLimboGhosts;
    } else {
        row = &gLimboGhosts[((entry->mode ? 6 : 0) + entry->player) * 3];
    }
    for (i = 0; i < gNumLimbo; i++) {
        h = row[i]->handle;
        if (h != 0 && entry->player == (*h)->player && entry->mode == (*h)->mode && !(entry->score < (*h)->score)) {
            break;
        }
    }
    i--;
    if (i >= 0) {
        if (row[0]->handle != 0) {
            release_handle(N64A(row[0]->handle));
        }
        if (i > 0) {
            game_memmove(row[0], row[1], i * sizeof(Record44));
        }
        row[i]->player = entry->player;
        row[i]->mode = entry->mode;
        memcpy(row[i]->name, entry->name, 13);
        memcpy(row[i]->ident, entry->ident, 8);
        row[i]->score = entry->score;
        row[i]->handle = AllocateHandleHeap(0, entry->bytes + sizeof(Header88));
        memcpy(*row[i]->handle, entry, sizeof(Header88));
        memcpy((u8 *) *row[i]->handle + sizeof(Header88), payload, entry->bytes);
        (*row[i]->handle)->offset = 0;
        (*row[i]->handle)->tail = 0;
        (*row[i]->handle)->head = (*row[i]->handle)->tail;
    }
}

static void release_handle(u32 address) {
    osRecvMesg((OSMesgQueue *) &heap_lock_queue, 0, 1);
    DeallocateMemoryInternal(address, 0);
    osSendMesg((OSMesgQueue *) &heap_lock_queue, 0, 0);
}
