/* Scorch marks on the track where weapons hit (MAT3 worlddam.c): a ring of 25 polygons, the oldest reused. */
#include "game/game.h"
#define tRGBA tRGBAView   /* this file's own tRGBA: include/game's tRGBA is another type */

typedef struct tPoly tPoly;
typedef struct tRGBA { u8 c[4]; } tRGBA;

/* The model record (MAT3 tModelData): each tyre's height off the ground. */
typedef struct tModelData {
    u8 unknown000[0x5ec];
    f32 airdist[4];
    u8 unknown5fc[0x808 - 0x5fc];
} tModelData;

extern surftyp *RoadPosition(f32 *, f32 *, f32 (*)[3]);

extern tPoly *ZOID_NewPoly(s32, f32 (*)[3], u16, tRGBA *, u16, s32);
extern void ZOID_UpdatePoly(tPoly *, s32, f32 (*)[3], u16, tRGBA *, u16, s32);

extern f32 fabsf(f32);

/* Returns the next scorch mark of the ring: a new polygon until there are 25, then the oldest, hidden. The four
 * after it in the ring (the oldest) get paler colours (damColor), so old marks fade. */
s32 findDamageSlot(void)
{
    s32 i;
    s32 j;
    s32 fade;

    if (DamagePolys.full) {
        ZOID_HidePoly(DamagePolys.polys[DamagePolys.oldest]);
        i = DamagePolys.oldest;
        if (++DamagePolys.oldest >= 25) {
            DamagePolys.oldest = 0;
        }
    } else {
        i = DamagePolys.oldest;
        DamagePolys.polys[i] = ZOID_NewPoly(4, dummyVtx, damTexture, damColor, 0x820f, 1);
        if (++DamagePolys.oldest >= 25) {
            DamagePolys.oldest = 0;
            DamagePolys.full = 1;
        }
    }
    for (j = 1; j < 5; j++) {
        fade = i + j;
        if (fade >= 25) {
            fade -= 25;
        }
        if (DamagePolys.polys[fade]) {
            ZOID_UpdatePoly(DamagePolys.polys[fade], 0, 0, 0, &damColor[j], 0, 0);
        }
    }
    return i;
}

/* Leaves a scorch mark 12 feet square on the road at `pos` (each corner put on the road a quarter foot up,
 * RoadPosition), unless car `slot` is in the air or the road there is too steep or broken (an edge rising more
 * than 3 feet or longer than about 14). The unused locals are the N64 code's. */
void newWorldDamage(f32 *pos, s32 slot)
{
    f32 newpos[3];
    f32 uvs[3][3];
    f32 xyz[4][3];
    f32 delta[3];
    f32 edge_length2;
    s32 damslot;
    s32 i;
    tModelData *m;
    s32 unused_0, unused_1, unused_2, unused_3, unused_4, unused_5, unused_6, unused_7, unused_8, unused_9;

    m = PC_MODEL(slot);
    if (m->airdist[0] > 8.0f || m->airdist[1] > 8.0f || m->airdist[2] > 8.0f || m->airdist[3] > 8.0f) {
        return;
    }
    for (i = 0; i < 4; i++) {
        xyz[i][0] = pos[0];
        xyz[i][1] = pos[1];
        xyz[i][2] = pos[2];
        xyz[i][1] += 3.0f;
        switch (i) {
        case 0:
            xyz[0][0] += -6.0f;
            xyz[0][2] += 6.0f;
            break;
        case 1:
            xyz[1][0] += -6.0f;
            xyz[1][2] += -6.0f;
            break;
        case 2:
            xyz[2][0] += 6.0f;
            xyz[2][2] += -6.0f;
            break;
        case 3:
            xyz[3][0] += 6.0f;
            xyz[3][2] += 6.0f;
            break;
        }
        if (RoadPosition(xyz[i], newpos, uvs)) {
            xyz[i][0] = newpos[0];
            xyz[i][1] = newpos[1];
            xyz[i][2] = newpos[2];
            xyz[i][1] += 0.25f;
        }
    }
    delta[0] = xyz[0][0] - xyz[1][0];
    delta[1] = xyz[0][1] - xyz[1][1];
    delta[2] = xyz[0][2] - xyz[1][2];
    if (fabsf(delta[1]) > 3.0f) return;
    edge_length2 = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
    if (edge_length2 > 200.0f) {
        return;
    }
    delta[0] = xyz[1][0] - xyz[2][0];
    delta[1] = xyz[1][1] - xyz[2][1];
    delta[2] = xyz[1][2] - xyz[2][2];
    if (fabsf(delta[1]) > 3.0f) return;
    edge_length2 = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
    if (edge_length2 > 200.0f) {
        return;
    }
    delta[0] = xyz[2][0] - xyz[3][0];
    delta[1] = xyz[2][1] - xyz[3][1];
    delta[2] = xyz[2][2] - xyz[3][2];
    if (fabsf(delta[1]) > 3.0f) return;
    edge_length2 = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
    if (edge_length2 > 200.0f) {
        return;
    }
    delta[0] = xyz[3][0] - xyz[0][0];
    delta[1] = xyz[3][1] - xyz[0][1];
    delta[2] = xyz[3][2] - xyz[0][2];
    if (fabsf(delta[1]) > 3.0f) return;
    edge_length2 = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
    if (edge_length2 > 200.0f) {
        return;
    }
    damslot = findDamageSlot();
    ZOID_UpdatePoly(DamagePolys.polys[damslot], 0, xyz, 0, damColor, 0x60f, 0);
}
