/* RenameBlit: give a blit another texture (MAT3 blit.c). */
#include "game/game.h"

/* The start of a texture's definition (MAT3 tTexture), all a blit reads of it. */
struct tTexture {
    char name[16];
    u16 width, height;
    u8 pad[16];
};

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
extern struct tTexture *MBOX_FindTexture_WORK(char *name, s16 *index, s8 lo, s8 hi, s32 err);

/* Makes blit show the texture name. With preserve the blit keeps its size, crop and colours and only the texture
 * changes (SelectBlit's strips of frames); without, InitBlit sets it up afresh at the new texture's size. */
void RenameBlit(tBlit *blit, char *name, s32 preserve) {
    struct tTexture *texture;

    blit->name = name;
    if (preserve) {
        if (!(texture = MBOX_FindTexture_WORK(blit->name, &blit->texIndex, 0, gNumModels - 1, MBOX_WARN))) {
        }
        blit->texture = texture;
    } else {
        InitBlit(blit);
    }
    UpdateBlit(blit);
}
