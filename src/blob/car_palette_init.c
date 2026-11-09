/* car_palette_init: a car's paint palette at a race's start. */
#include "game/game.h"

/* PC: the car palette descriptors car_palettes have a WAD palette record's (PLHD) N64 layout, which the model draw
 * reads them through: data is a u32 window address (a heap block). docs/port/LOADERS.md */
#define DESC_PTR(name) u32 name
/* ... so a block's address goes in as its 32-bit N64 address: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
/* A palette descriptor (a WAD palette record, PLHD: MAT3 anon_name_start_18's shape). */
typedef struct Slot24 { u8 flag; u8 other1[15]; u8 active, marker; u16 value; DESC_PTR(data); } Slot24;

VIEW(Slot24[], car_palettes);
#define car_palettes car_palettes_view

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
        car_palettes[index].data = N64A(data);
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
        car_palettes[index].data = N64A(AllocateMemoryHeap(0, 512));
    }
}
