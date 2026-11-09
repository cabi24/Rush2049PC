/* car_palette_init: a car's paint palette at a race's start. */
#include "game/game.h"

/* Sets car `index`'s palette (car_palettes[index], a palette record the model draw loads, tPaletteDef) to the whole
 * TLUT, entries 0 to 255, with the colours `data`, or a new 512-byte block of the heap; with the heap too short, it
 * shares the colours of an earlier car of the same `kind` (the roster's car), and with none, the car has no colours
 * of its own.
 * PC: the palette is MAT3's record with a pointer to its colours, where the N64 kept their 32-bit address in the
 * PLHD record's layout (docs/port/NATIVE.md step 7); the heap too short for the paint is logged (port/ultra/heap.c). */
void car_palette_init(int index, int kind, void *data)
{
    int i;

    car_palettes[index].name[0] = 0;
    car_palettes[index].start = 0;
    car_palettes[index].end = 255;
    car_palettes[index].format = 0x8000;
    if (data != 0) {
        car_palettes[index].palBase = data;
    } else if (heap_total_free(0) < 512) { pc_heap_short("a car's paint", 512);
        for (i = 0; i < index; i++) {
            if (kind == PC_LINK_CAR(i)) {
                car_palettes[index].palBase = car_palettes[i].palBase;
                PC_LINK_COLOR0(index) = PC_LINK_COLOR0(i);
                PC_LINK_COLOR1(index) = PC_LINK_COLOR1(i);
                PC_LINK_COLOR2(index) = PC_LINK_COLOR2(i);
                break;
            }
        }
        if (i == index) {
        }
    } else {
        car_palettes[index].palBase = AllocateMemoryHeap(0, 512);
    }
}

int osRecvMesg(OSMesgQueue *, void **, int);
int osSendMesg(OSMesgQueue *, void *, int);

/* Frees car `index`'s colours (car_palettes[index].palBase, a heap block), under the heap lock. (heap_release/group.c
 * had this function until the heap took pointers, docs/port/NATIVE.md step 6.) */
void free_car_palette(s32 index) {
    void *block = car_palettes[index].palBase;

    osRecvMesg(&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(block, 0);
    osSendMesg(&heap_lock_queue, NULL, 0);
}
