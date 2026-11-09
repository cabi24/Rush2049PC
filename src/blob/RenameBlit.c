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

void RenameBlit(Blit *blit, char *name, s32 preserve) {
    TexDef *ti;

    blit->name = name;
    if (preserve) {
        if (!(ti = MBOX_FindTexture_WORK(blit->name, &blit->texIndex, 0, gNumModels - 1, MBOX_WARN))) {
        }
        blit->info = ti;
    } else {
        InitBlit(blit);
    }
    UpdateBlit(blit);
}
