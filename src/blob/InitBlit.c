#include "game/game.h"

typedef struct TexDef {
    char name[16];
    u16 width, height;
    u8 pad[16];
} TexDef;

typedef struct Blit {
    char *name;
    void *image;
    TexDef *info;
    s16 texIndex;
    u8 unk0E[4];
    u16 state;
    u16 width, height;
    u8 alpha;
    u8 unk19[3];
    s16 top, bot, left, right, color;
} Blit;

#define MBOX_WARN 1

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
extern TexDef *MBOX_FindTexture_WORK(char *name, s16 *index, s8 lo, s8 hi, s32 err);

void InitBlit(Blit *blit) {
    if (blit->name == 0) {
        blit->texIndex = 0;
        blit->info = 0;
        blit->width = 0;
        blit->height = 0;
        blit->image = default_blit_image;
    } else if (blit->name == (char *)-1) {
        blit->texIndex = 0;
        blit->info = 0;
        blit->width = 0;
        blit->height = 0;
        blit->image = 0;
    } else {
        blit->info = MBOX_FindTexture_WORK(blit->name, &blit->texIndex, 0, gNumModels - 1, MBOX_WARN);
        blit->width = blit->info->width;
        blit->height = blit->info->height;
        blit->image = 0;
    }
    blit->state = 0;
    blit->alpha = 255;
    blit->top = -1;
    blit->bot = -1;
    blit->left = -1;
    blit->right = -1;
    blit->color = -1;
}
