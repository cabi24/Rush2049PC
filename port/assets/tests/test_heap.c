/* port/assets/tests/test_heap.c: the game heap's resize in place (menu_item_select, heap_release; the save code
 * resizes a ghost's header block with it) through the game's own heap code, whose block and heap headers keep their
 * N64 layout in the RDRAM window (magic, next, prev, size, owner, used, tag: 32-bit links; docs/port/LAYOUT.md 5).
 * A block shrunk with its successor in use gets a free block split off after it; one shrunk next to a free block
 * moves that block's header down; one grown into a free successor takes part of it, or all of it with what is left
 * under a header's size. The heap is the game's (func_800E7D0C, AllocateMemoryHeap) in a window the test maps
 * (exit 77, skipped, if it can't).
 * game: src/blob/groups/audio_heap/group.c src/blob/groups/heap_release/group.c src/blob/func_80095EF4.c
 * game: src/blob/func_80095F8C.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define ABS(name, addr) __asm__(".globl " #name "\n.set " #name ", " #addr)
#define W32(p) ((uint32_t)(uintptr_t)(p))
#define WP(a) ((uint8_t *)(uintptr_t)(a))
#define IN_WINDOW(p) ((uintptr_t)(p) >= 0x80000000u && (uintptr_t)(p) < 0x80800000u)
#define U32(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))
#define MAGIC 0xFEDCBA98u
/* a block header: magic +0, next +4, prev +8, size +0xC, owner +0x10, used +0x14, tag +0x15; the data at +0x20 */
#define NEXT(h) WP(U32(h, 4))
#define PREV(h) WP(U32(h, 8))
#define SIZE(h) U32(h, 0xC)
#define USED(h) ((h)[0x14])

/* ---------------------------------------------------------------- the game's data */

void *D_801527C8;                               /* the heap */
int32_t D_801527A0;
uint8_t D_80152770[32];
int8_t D_80156994;
ABS(D_8017A640, 0x8017A640);                    /* the end of the game's BSS: the heap starts there */
ABS(D_80000318, 0x80000318);                    /* osMemSize */
/* what heap_release's other functions reach */
int8_t D_8011ED04, D_8011ED00;
uint8_t D_8038A400[16];
uint32_t D_8013FEF4[16 * 6];
uintptr_t D_8011025C, D_80110260;

/* ---------------------------------------------------------------- what the code under test calls */

void osCreateMesgQueue(void *q, void *m, int32_t n) {}
int32_t osSendMesg(void *q, void *m, int32_t flags) { return 0; }
int32_t osRecvMesg(void *q, void **m, int32_t flags) { return 0; }
void func_80095EC0(void *p, uint32_t n) {}
void *n64_memset(void *p, int c, size_t n) { return memset(p, c, n); }

void func_800E7D0C(uint16_t count, uint16_t a);
void *AllocateMemoryHeap(void *heap, uint32_t size);
void menu_item_select(uint32_t address, uint32_t desired);

int main(void)
{
    uint8_t *w = test_rdram(), *a, *b, *c, *ha, *hb, *hc, *hm, *after;
    uint32_t rest;

    if (!w) {
        fprintf(stderr, "test_heap: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    pc_fix_absolute_got();
    U32(w, 0x318) = 0x400000;                   /* osMemSize: 4 MB */
    func_800E7D0C(16, 16);
    a = AllocateMemoryHeap(NULL, 256);
    b = AllocateMemoryHeap(NULL, 128);
    c = AllocateMemoryHeap(NULL, 512);
    ha = a - 32, hb = b - 32, hc = c - 32;
    CHECK(IN_WINDOW(a) && IN_WINDOW(c) && U32(ha, 0) == MAGIC && NEXT(ha) == hb && PREV(hb) == ha && SIZE(ha) == 256);

    /* shrink a: b is in use, so a free block is split off after a */
    menu_item_select(W32(a), 64);
    hm = ha + 32 + 64;
    CHECK(SIZE(ha) == 64 && NEXT(ha) == hm && U32(hm, 0) == MAGIC && PREV(hm) == ha && NEXT(hm) == hb);
    CHECK(PREV(hb) == hm && SIZE(hm) == 256 - 64 - 32 && USED(hm) == 0 && U32(hm, 0x10) == 0 && USED(ha) == 1);

    /* shrink c: its successor is the free rest of the heap, whose header moves down */
    after = NEXT(hc);
    CHECK(IN_WINDOW(after) && USED(after) == 0 && PREV(after) == hc);
    rest = SIZE(after);
    menu_item_select(W32(c), 256);
    hm = hc + 32 + 256;
    CHECK(SIZE(hc) == 256 && NEXT(hc) == hm && PREV(hm) == hc && SIZE(hm) == 512 - 256 + rest && USED(hm) == 0);

    /* grow c by 128: the free successor gives that much */
    menu_item_select(W32(c), 384);
    hm = hc + 32 + 384;
    CHECK(SIZE(hc) == 384 && NEXT(hc) == hm && PREV(hm) == hc && SIZE(hm) == 512 - 384 + rest);
    CHECK(U32(hm, 0) == MAGIC && USED(hm) == 0 && USED(hc) == 1);
    return TEST_DONE();
}
