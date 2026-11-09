#include "game/game.h"

typedef struct NameEntry { char name[36]; } NameEntry;

VIEW(s16, damTexture);
#define damTexture damTexture_view

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
extern NameEntry *MBOX_FindTexture_WORK(char *, s16 *, s8, s8, s32);
void initWorldDamage(void)
{
    s32 i;
    DamagePolys.wrapped = 0;
    DamagePolys.next = 0;
    MBOX_FindTexture_WORK(scorch_texture_name, &damTexture, 0, gNumModels - 1, 1);
    for (i = 0; i < 25; i++) {
        DamagePolys.objects[i] = 0;
    }
}
