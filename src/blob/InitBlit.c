/* InitBlit: a blit's texture and defaults from its name (MAT3 blit.c). */
#include "game/game.h"

/* The start of a texture's definition (MAT3 tTexture), all a blit reads of it. */
struct tTexture {
    char name[16];
    u16 width, height;
    u8 pad[16];
};

#define MBOX_WARN 1 /* MBOX_FindTexture_WORK: complain when the texture isn't found */
extern struct tTexture *MBOX_FindTexture_WORK(char *name, s16 *index, s8 lo, s8 hi, s32 err);

/* Sets up blit from its name: a NULL name is a plain coloured rectangle (default_blit_image's colour, no size yet),
 * BLIT_NO_TEXTURE a text blit (nothing), any other the texture of that name in the loaded models, at its own size.
 * Then the defaults: depth 0, opaque, the whole texture shown, no colour. */
void InitBlit(tBlit *blit) {
    if (blit->name == 0) {
        blit->texIndex = 0;
        blit->texture = 0;
        blit->width = 0;
        blit->height = 0;
        blit->palette = default_blit_image;
    } else if (blit->name == BLIT_NO_TEXTURE) {
        blit->texIndex = 0;
        blit->texture = 0;
        blit->width = 0;
        blit->height = 0;
        blit->palette = 0;
    } else {
        blit->texture = MBOX_FindTexture_WORK(blit->name, &blit->texIndex, 0, gNumModels[0] - 1, MBOX_WARN);
        blit->width = blit->texture->width;
        blit->height = blit->texture->height;
        blit->palette = 0;
    }
    blit->z = 0;
    blit->alpha = 255;
    blit->top = -1;
    blit->bot = -1;
    blit->left = -1;
    blit->right = -1;
    blit->color = -1;
}
