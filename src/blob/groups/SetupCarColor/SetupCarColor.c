/* SetupCarColor.c: a car's paint job, made in its palette (MAT3 visuals.c, SetupCarColor()).
 *
 * The N64's cars are painted through their palette (TLUT): the three colours a player picks become 32-step ramps
 * in a copy of the car type's palette, which the frame and the hood draw with. MAT3 swaps textures instead. */
#include "game/game.h"

/* PC: the WAD's TLUT (a palette record's colours: MBOX_FindPalette_WORK) keeps the N64's texel format in the file; the
 * car's copy, in a heap block that port/gfx reads natively, is made native as it is read. */
#define TLUT_COPY_IN(dst, src, size) tlut_copy_in(dst, src, size)
/* Copy `size` bytes of a file's RGBA 5551 entries from `src` to `dst`, native. */
static void tlut_copy_in(u16 *dst, const u8 *src, u32 size)
{
    u32 i;
    for (i = 0; i < size / 2; i++)
        dst[i] = pc_tlut_get(src + 2 * i);
}
/* PC: the car part's s32 model handle (MBOX_NewObject's), and id its low half: the second s16 on the N64, the first on a
 * little-endian host (docs/port/ENDIAN.md). */
extern s32 sprintf(char *, char *, ...);
extern void *memcpy(void *, const void *, u32);

int osRecvMesg(OSMesgQueue *, void **, int);
int osSendMesg(OSMesgQueue *, void *, int);
#define heap_lock() osRecvMesg(&heap_lock_queue, 0, 1)
#define heap_unlock() osSendMesg(&heap_lock_queue, 0, 0)

/* The RGBA 5551 colour `ratio`/255 of the way from `left` to `right` (0 gives left, 255 or more right), each of red,
 * green and blue blended apart; alpha 1. */
u32 blend_rgb5551(u32 left, u32 right, s32 ratio)
{
    u32 inverse, red, green, blue;
    if (ratio == 0) return left;
    if (ratio >= 255) return right;
    inverse = 255U - (u32)ratio;
    red = ((s32)((left & 0xF800) * inverse + (right & 0xF800) * (u32)ratio) >> 8) & 0xF800;
    green = ((s32)((left & 0x7C0) * inverse + (right & 0x7C0) * (u32)ratio) >> 8) & 0x7C0;
    blue = ((s32)((left & 0x3E) * inverse + (right & 0x3E) * (u32)ratio) >> 8) & 0x3E;
    return red | green | blue | 1;
}

/* Draw scene node `id` with the palette `descriptor`. */
void SetModelPalette(s32 id, tPaletteDef *descriptor)
{
    ObjectInstance *m;
    m = &ObjInstanceList[id];
    m->palette = descriptor;
}

/* Write the three paint ramps into the palette `colors`: entry 32 the first colour, then 31 steps from it toward
 * black (blend_rgb5551 with 1, black with alpha), 64 on the second colour, 96 on the third; returns colors.
 * Original game: the third ramp stops at entry 126, one short of the others, so 127 keeps the palette file's. */
u16 *MakeRamps(u16 *colors, u32 first_color, u32 second_color, u32 third_color)
{
    s32 i;

    colors[32]=first_color;
    for(i=33;i<64;i++) {
        colors[i]=blend_rgb5551(first_color, 1,(i-33)*8);
    }
    colors[64]=second_color;
    for(i=65;i<96;i++) {
        colors[i]=blend_rgb5551(second_color, 1,(i-65)*8);
    }
    colors[96]=third_color;
    for(i=97;i<127;i++) {
        colors[i]=blend_rgb5551(third_color, 1,(i-97)*8);
    }
    return colors;
}

/* Paint car `slot` (of car type `car`) in the paint colours `first`, `second` and `third` (paint_colors numbers):
 * read the car type's palette ("car_palette_name_format" with car + 1) into a heap block, make it native, make
 * the ramps, copy the result into the car's palette record (car_palettes[slot], car_palette_init) and give it to
 * the frame and the hood. Colour 0 for the first paint makes the palette's first 31 entries a brighter grey (their
 * red, a quarter more).
 * Original game: with `second` 0 it greys entries 33-63 the same way, which MakeRamps then overwrites with the
 * first colour's ramp, so it does nothing; 65-95, the second ramp's, may have been meant. */
void SetupCarColor(s16 slot,s16 car,u8 first,u8 second,u8 third)
{
    char name[32];
    u16 *colors;
    u16 handle;
    s32 i, value;
    s32 r, g, b;
    u32 first_color, second_color, third_color;
    tPaletteDef *descriptor;
    gCarPaintJobInited[slot]=1;
    colors=AllocateMemoryHeap(0,512);
    sprintf(name,car_palette_name_format,car+1);
    TLUT_COPY_IN(colors,MBOX_FindPalette_WORK(name,&handle,0,gNumModels[0]-1)->palBase,512);
    if (first == 0) {
        for (i = 1; i < 32; i++) {
            value = (s32)((((colors[i] >> 11) & 0x7f) << 3) * 1.25f);
            if (value > 255) value = 255;
            r = (value << 8) & 0xf800;
            g = (value << 3) & 0x7c0;
            b = (value >> 2) & 0x3e;
            colors[i] = (u16)(r | g | b) | 1;
        }
    } else if (second == 0) {
        for (i = 33; i < 64; i++) {
            value = (s32)((((colors[i] >> 11) & 0x7f) << 3) * 1.25f);
            if (value > 255) value = 255;
            r = (value << 8) & 0xf800;
            g = (value << 3) & 0x7c0;
            b = (value >> 2) & 0x3e;
            colors[i] = (u16)(r | g | b) | 1;
        }
    }
    first_color=paint_colors[first];
    second_color=paint_colors[second]; third_color=paint_colors[third];
    MakeRamps(colors, first_color, second_color, third_color);
    descriptor=&car_palettes[slot];
    memcpy(descriptor->palBase,colors,512);
    heap_lock();
    DeallocateMemoryInternal(colors, 0);
    heap_unlock();
    SetModelPalette((s16)gCarParts[slot].frame,descriptor);
    SetModelPalette((s16)gCarParts[slot].hood,descriptor);
}
