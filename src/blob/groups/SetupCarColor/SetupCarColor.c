#include "game/game.h"
#define Handle HandleView   /* this file's own Handle: include/game's Handle is another type */

/* PC: Resource24 is a palette record, a WAD's PLHD (MBOX_FindPalette_WORK) or a car's descriptor car_palettes, both
 * in the N64 layout (port/assets/wad.c): data is a 32-bit window address. The WAD's TLUT stays big-endian; the
 * car's copy, in a heap block that port/gfx reads natively, is made native as it is read. */
#define PAL_DATA u32 data
#define TLUT_COPY_IN(dst, src, size) tlut_copy_in(dst, src, size)
static void tlut_copy_in(u16 *dst, u32 src, u32 size)
{
    const u8 *s = (const u8 *)PC_N64P(src);
    u32 i;
    for (i = 0; i < size / 2; i++)
        dst[i] = pc_be16_get(s + 2 * i);
}
/* The car's copy and the heap's API take 32-bit N64 addresses: N64A(p) is a window pointer's, N64PTR0(T, a) the
 * pointer at one (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
#define N64PTR0(T, a) ((T)PC_N64P(a))
typedef struct Resource24 { u8 directory_fields[20]; PAL_DATA; } Resource24;
/* PC: the car part's s32 model handle (MBOX_NewObject's), and id its low half: the second s16 on the N64, the first on a
 * little-endian host (docs/port/ENDIAN.md). */
typedef union Handle { s32 value; struct { s16 id, bank; } halves; } Handle;
typedef struct Handles64 { Handle root; u32 unknown04[3]; Handle secondary, body; u32 unknown18[10]; } Handles64;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Model68 { u32 flags, w4; void *obj; float scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip; Resource24 *palette; u32 texture0, texture1; } Model68;

VIEW(Resource24[], car_palettes);
#define car_palettes car_palettes_view
VIEW(Handles64[], gCarParts);
#define gCarParts gCarParts_view
VIEW(Model68[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

extern Resource24 *MBOX_FindPalette_WORK(char *, u16 *, s8, s8, s32);    /* texture lookup */
extern s32 sprintf(char *, char *, ...);
extern void *memcpy(void *, const void *, u32);

VIEW(struct OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view
int osRecvMesg(struct OSMesgQueue *, void **, int);
int osSendMesg(struct OSMesgQueue *, void *, int);
#define heap_lock() osRecvMesg(&heap_lock_queue, 0, 1)
#define heap_unlock() osSendMesg(&heap_lock_queue, 0, 0)

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


void SetModelPalette(s32 id, Resource24 *descriptor)
{
    Model68 *m;
    m = &ObjInstanceList[id];
    m->palette = descriptor;
}



u16 *MakeRamps(u16 *colors, u32 first_color, u32 second_color, u32 third_color)   /* MakeRamps */
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

void SetupCarColor(s16 index,s16 palette,u8 first,u8 second,u8 third)
{
    char name[32];
    u16 *colors;
    u16 handle;
    s32 i, value;
    s32 r, g, b;
    u32 first_color, second_color, third_color;
    Resource24 *descriptor;
    gCarPaintJobInited[index]=1;
    colors=AllocateMemoryHeap(0,512);
    sprintf(name,car_palette_name_format,palette+1);
    TLUT_COPY_IN(colors,MBOX_FindPalette_WORK(name,&handle,0,gNumModels-1,1)->data,512);
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
    descriptor=&car_palettes[index];
    memcpy(N64PTR0(void *, descriptor->data),colors,512);
    heap_lock();
    DeallocateMemoryInternal(N64A(colors), 0);
    heap_unlock();
    SetModelPalette(gCarParts[index].root.halves.id,descriptor);
    SetModelPalette(gCarParts[index].secondary.halves.id,descriptor);
}
