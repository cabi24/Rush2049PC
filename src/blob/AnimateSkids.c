#include "game/game.h"

typedef struct Voice Voice;
struct Voice {
    Voice *next;
    Voice *reserved4;   /* the doubly linked list's back link (GetStatLLE), pointer-sized here */
    f32 start;
    intptr_t handle;
    u8 pad10[12];
    u8 bytes[4];
    s32 enabled;
};
/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct VoiceLists {
    u8 flag;
    s32 count, size;
    void *mem;
    Voice *active;
    Voice *other;
} VoiceLists;
VIEW(VoiceLists, gSkidList);
#define gSkidList gSkidList_view

extern void ZOID_UpdatePoly(intptr_t, s32, s32, s32, u8 *, s32, s32);
extern void ZOID_RemovePoly(intptr_t);

/* PC: a float to u32 that is negative while a voice's start is ahead of the clock. Undefined in C, and the hosts
 * differ: x86-64 and IDO's code for the N64 go through a signed conversion and keep the bits (-125 is 0xFFFFFF83, no
 * fade), aarch64's fcvtzu gives 0 (the fade at full). So through s32 below 0, as the N64 (docs/port/SANITIZE.md). */
static u32 pc_f2u(f32 x) { return x < 0.0f ? (u32)(s32)x : (u32)x; }
#define F2U pc_f2u
void AnimateSkids(void) {
    Voice *p, *next;
    s32 count = 0;
    u32 amount;
    f32 rate;
    for (p = gSkidList.other; p; p = p->next) count++;
    p = gSkidList.active;
    while (p) {
        if (!p->enabled) {
            p = p->next;
            continue;
        }
        {
            rate = (gSeconds - p->start) / (((f32)count + (f32)count)/100.0f + 2.0f);
            amount = F2U(rate * 255.0f + 63.0f);
            if (amount < 255) {
                p->bytes[3] = 255 - amount;
                if (p->bytes[3] > 192) p->bytes[3] = 192;
                ZOID_UpdatePoly(p->handle, 4, 0, 0, p->bytes, 0, 0);
                p = p->next;
            } else {
                next = p->next;
                ZOID_RemovePoly(p->handle);
                FreeStatLLE(&gSkidList, p);
                p = next;
            }
        }
    }
}
