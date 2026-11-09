/* ZOID_AddBlit: take an entry of the zoid blit list (MAT3 zoidblit.c). */
#include "game/game.h"

VIEW(tBlitInstance[MAX_BLITS], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view

/* Hands out the first free entry of the zoid blit list (a freed one, else a new one at the end: gNumBlitInsts grows,
 * gMaxBlitInsts keeps the most ever used) and fills it: texture texIndex/texture, colours palette, position x, y,
 * size width x height, depth 0, opaque, the whole texture, shown, no flags. Returns its index, or -1 when all
 * MAX_BLITS are taken. (The N64 fills the fields itself where MAT3 calls ZOID_UpdateBlit.) */
s32 ZOID_AddBlit(s32 texIndex, void *texture, void *palette, s32 x, s32 y, s32 width, s32 height)
{
    tBlitInstance *inst;
    s32 i;

    for (i = 0; i < gNumBlitInsts; i++) {
        if (gBlitInstanceList[i].hide == BLITINST_FREE) {
            break;
        }
    }
    if (i >= MAX_BLITS) {
        return -1;
    }
    inst = &gBlitInstanceList[i];
    if (i >= gNumBlitInsts) {
        gNumBlitInsts++;
    }
    if (gMaxBlitInsts < gNumBlitInsts) {
        gMaxBlitInsts = gNumBlitInsts;
    }

    inst->y = y;
    inst->texture = texture;
    inst->palette = palette;
    inst->texIndex = texIndex;
    inst->x = x;
    inst->z = 0;
    inst->alpha = 255;
    inst->top = 0;
    inst->left = 0;
    inst->width = width;
    inst->height = height;
    do {} while (0);
    inst->bot = height - 1;
    inst->right = width - 1;
    inst->hide = 0;
    inst->flags = 0;

    return i;
}
