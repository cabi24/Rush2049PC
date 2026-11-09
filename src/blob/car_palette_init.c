/* car_palette_init: a car's paint palette at a race's start. */
#include "game/game.h"

/* PC: the car palette descriptors car_palettes have a WAD palette record's (PLHD) N64 layout, which the model draw
 * reads them through: data is a u32 window address (a heap block, docs/port/LOADERS.md), so a block's address goes
 * in as its 32-bit N64 address: PC_N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */

/* PC: the heap too short for the paint, where a car shares another's, is logged (port/ultra/heap.c). */

/* Sets car `index`'s palette descriptor (car_palettes[index]) to `data`, or a new 512-byte block of the heap; with
 * the heap too short, it shares the palette and colours of an earlier car of the same `kind` (the roster's car),
 * and with none, the car has no palette of its own. */
void car_palette_init(int index, int kind, void *data)
{
    int i;

    car_palettes[index].flag = 0;
    car_palettes[index].active = 0;
    car_palettes[index].marker = 255;
    car_palettes[index].value = 0x8000;
    if (data != 0) {
        car_palettes[index].data = PC_N64A(data);
    } else if (heap_total_free(0) < 512) { pc_heap_short("a car's paint", 512);
        for (i = 0; i < index; i++) {
            if (kind == PC_LINK_CAR(i)) {
                car_palettes[index].data = car_palettes[i].data;
                PC_LINK_COLOR0(index) = PC_LINK_COLOR0(i);
                PC_LINK_COLOR1(index) = PC_LINK_COLOR1(i);
                PC_LINK_COLOR2(index) = PC_LINK_COLOR2(i);
                break;
            }
        }
        if (i == index) {
        }
    } else {
        car_palettes[index].data = PC_N64A(AllocateMemoryHeap(0, 512));
    }
}

int osRecvMesg(OSMesgQueue *, void **, int);
int osSendMesg(OSMesgQueue *, void *, int);

/* Frees car `index`'s palette block (car_palettes[index].data), under the heap lock. The descriptor's data is the
 * block's window address (its PLHD layout, above), and the heap takes the block's pointer. (heap_release/group.c had
 * this function until the heap took pointers, docs/port/NATIVE.md step 6.) */
void free_car_palette(s32 index) {
    void *block = (void *)PC_N64P(car_palettes[index].data);

    osRecvMesg(&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(block, 0);
    osSendMesg(&heap_lock_queue, NULL, 0);
}
