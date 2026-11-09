/* RenameBlit: give a blit another texture (MAT3 blit.c). */
#include "game/game.h"

/* Makes blit show the texture name. With preserve the blit keeps its size, crop and colours and only the texture
 * changes (SelectBlit's strips of frames); without, InitBlit sets it up afresh at the new texture's size. */
void RenameBlit(tBlit *blit, char *name, s32 preserve) {
    struct tTexture *texture;

    blit->name = name;
    if (preserve) {
        if (!(texture = MBOX_FindTexture_WORK(blit->name, &blit->texIndex, 0, gNumModels[0] - 1, MBOX_WARN))) {
        }
        blit->texture = texture;
    } else {
        InitBlit(blit);
    }
    UpdateBlit(blit);
}
