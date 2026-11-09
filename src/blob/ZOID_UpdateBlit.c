/* ZOID_UpdateBlit: a zoid blit list entry's texture, colours, position and size (MAT3 zoidblit.c). */
#include "game/game.h"

VIEW(tBlitInstance[], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view

/* Sets entry index's texture (texIndex, texture), colours (palette), position (x, y) and size (width, height). */
void ZOID_UpdateBlit(u32 index, u32 texIndex, void *texture, void *palette, u32 x, u32 y, u32 width, u32 height)
{
    tBlitInstance *inst = &gBlitInstanceList[index];
    inst->texture = texture;
    inst->palette = palette;
    inst->texIndex = texIndex;
    inst->x = x;
    inst->y = y;
    inst->width = width;
    inst->height = height;
}
