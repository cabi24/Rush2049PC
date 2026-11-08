/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * car_palette_init(index, kind, data) -- initialise the 24-byte per-node slot
 * car_palettes[index] (clear flag/active, marker 255, value 0x8000) and give it a
 * 512-byte buffer: the caller's `data` if supplied; otherwise, when the shared
 * heap has less than 512 bytes free (heap_total_free(NULL) = free total), share
 * the buffer of the first earlier node whose gLink entry (D_80153E88, 8 bytes,
 * as in munge_gLink_data) has the same kind byte and copy that node's three payload
 * bytes; else allocate with audio_dma_sync(0, 512).  Caller: audio_interrupt_handler.
 * No arcade ancestor found.
 *
 * Shaping (prior research had the logic):
 *  - no `slot` pointer local: every access is car_palettes[index].field, so the
 *    slot address is a spilled temporary (frame 32, home 24), not a named local (frame 40);
 *  - the shared buffer pointer is copied before the payload bytes;
 *  - `if (i == index) {}` after the loop is compiled-out code: it is retail's
 *    `bnel v1,a3` / `b` pair at the loop exit.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct Owner Owner;
/* PC: the car palette descriptors car_palettes have a WAD palette record's (PLHD) N64 layout, which the model draw
 * reads them through: data is a u32 window address (a heap block). docs/port/LOADERS.md */
#define DESC_PTR(name) u32 name
/* ... so a block's address goes in as its 32-bit N64 address: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
typedef struct Slot24 { u8 flag; u8 other1[15]; u8 active, marker; u16 value; DESC_PTR(data); } Slot24;
typedef struct Link { u8 state; u8 kind; u8 payload[3]; u8 other5[3]; } Link;

extern Slot24 car_palettes[];
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern u32 heap_total_free(Owner *);
extern void *AllocateMemoryHeap(int, int);

/* PC: the heap too short for the paint, where a car shares another's, is logged (port/ultra/heap.c). */

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
