/* initWorldDamage: the scorch marks on the track at its set-up (MAT3 worlddam.c). */
#include "game/game.h"

typedef struct NameEntry { char name[36]; } NameEntry;

VIEW(s16, damTexture);
#define damTexture damTexture_view

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
/* Empties the ring of 25 scorch marks (DamagePolys) and finds the scorch texture. */
void initWorldDamage(void)
{
    s32 i;
    DamagePolys.full = 0;
    DamagePolys.oldest = 0;
    MBOX_FindTexture_WORK(scorch_texture_name, &damTexture, 0, gNumModels - 1, 1);
    for (i = 0; i < 25; i++) {
        DamagePolys.polys[i] = 0;
    }
}
