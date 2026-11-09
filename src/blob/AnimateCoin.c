/* AnimateCoin: the animation of the gold and silver coins on the stunt and obstacle tracks (MAT3 targets.c
 * _AnimateCoin). It is the coin's environment visual, called each frame: the first time it numbers the coin, makes
 * its spinning model and hides it from the players who have already found it; then it spins the coin, and once
 * every player has it (StartCoin marks that) it removes the coin. */
#include "game/game.h"

typedef f32 Mat3[3][3];

/* A scene object's handle, the s32 MBOX_NewObject returns; the object's index is its low half, which the ZOID
 * functions take: the second s16 on the N64, the first on a little-endian host (docs/port/ENDIAN.md). */
typedef union SceneHandle {
    s32 handle;
    struct { s16 index, high; } part;
} SceneHandle;

/* A coin's own record (MAT3 tTargetStructCoin, 0x4C bytes), the target's per-kind data. */
typedef struct CoinData {
    SceneHandle dlIdx;      /* 0x00 the spinning coin's scene object */
    Mat3 uvs;               /* 0x04 mat: its orientation */
    f32 pos[3];             /* 0x28 mat: its position */
    u8 pad34[8];            /* 0x34 glowTime, sc: unused here */
    f32 angV[3];            /* 0x3C its spin, radians a second about x, y, z */
    u16 cameras;            /* 0x48 a bit per player who hasn't found it */
    s16 whichcoin;          /* 0x4A its number in the profile's found-coin bits (gold coins from 8) */
} CoinData;

/* The coin's glow, its target's scene object (tTarget.dlIdx): the object's index is the handle's low half. */
#define GLOW_INDEX(t) ((s16) (t)->dlIdx)

/* A player's window bit; a view of -1 shifts by a negative count, undefined in C. The N64's sllv, x86-64's shl and
 * aarch64's lsl all take the count's low 5 bits, so this does (docs/port/SANITIZE.md). */
#define VIEW_BIT(s) 1 << ((s) & 31)

/* One frame of a coin (op 1), or release its visual (op 0). Gold coins are numbered from 8 (gWhichGoldCoin + 8),
 * silver ones from 0, in the order their visuals first run; a coin every player has already found is removed at
 * once. Nothing moves while the game is paused. The visual's data is the coin's target, whose data is the coin's
 * record, its dlIdx the coin's glow. */
void AnimateCoin(struct pc_visual *v, s16 op)
{
    tTarget *t;
    CoinData *coin;
    f32 spin[3];
    s32 number, i, foundByAll;
    struct pc_car *car;
    u32 goldGlow[1], silverGlow[1];
    f32 *angV;

    if (!op) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState) return;
    t = (tTarget *) v->data;
    coin = t->data;
    if (t->Flags & TARGET_COIN_GONE) {
        RemoveObjectTree(coin->dlIdx.part.index, 0, 0);
        RemoveObjectTree(GLOW_INDEX(t), 0, 0);
        FreeStatLLE(&gTargetList, t);
        ReleaseEnviron(v, 1);
        return;
    }
    if (!(t->Flags & TARGET_SET_UP)) {
        goldGlow[0] = PC_RGBA(coin_gold_glow_color);
        silverGlow[0] = PC_RGBA(coin_silver_glow_color);
        if (t->Flags & TARGET_GOLD) {
            number = gWhichGoldCoin + 8;
            coin->whichcoin = number;
            gWhichGoldCoin++;
        } else {
            number = gWhichSilverCoin;
            coin->whichcoin = number;
            gWhichSilverCoin++;
        }
        ZOID_SetObjectDef(GLOW_INDEX(t), MBOX_FindObject_WORK(coin_glow_object_name, 0, gNumModels[0] - 1, 1));
        coin->angV[0] = 0.0f;
        coin->angV[2] = 0.0f;
        coin->angV[1] = 3.0f;
        CopyUV(unitmatrix, coin->uvs);
        coin->pos[0] = t->mat.pos[0];
        coin->pos[1] = t->mat.pos[1];
        coin->pos[2] = t->mat.pos[2];
        if (t->Flags & TARGET_GOLD) {
            coin->dlIdx.handle = ZOID_AddObject(MBOX_FindObject_WORK(gold_coin_object_name, 0, gNumModels[0] - 1, 1), coin->uvs, -1, 0x40000);
            ZOID_SetColor(GLOW_INDEX(t), goldGlow);
        } else {
            coin->dlIdx.handle = ZOID_AddObject(MBOX_FindObject_WORK(silver_coin_object_name, 0, gNumModels[0] - 1, 1), coin->uvs, -1, 0x40000);
            ZOID_SetColor(GLOW_INDEX(t), silverGlow);
        }
        coin->cameras = 0;
        foundByAll = 1;
        for (i = 0; i < gNumPlayers; i++) {
            if (!IsCoinFound(i, number)) {
                foundByAll = 0;
                coin->cameras |= 1 << i;
            } else {
                car = PC_CAR(i);
                MBOX_HideObject(t->dlIdx, 1, VIEW_BIT(car->camera));
                MBOX_HideObject(coin->dlIdx.handle, 1, VIEW_BIT(car->camera));
            }
        }
        if (foundByAll) {
            RemoveObjectTree(coin->dlIdx.part.index, 0, 0);
            RemoveObjectTree(GLOW_INDEX(t), 0, 0);
            FreeStatLLE(&gTargetList, t);
            ReleaseEnviron(v, 1);
            return;
        }
        t->Flags |= TARGET_SET_UP;
    }
    angV = ((CoinData *) t->data)->angV;
    spin[0] = angV[0] * gFrameSeconds;
    spin[1] = angV[1] * gFrameSeconds;
    spin[2] = angV[2] * gFrameSeconds;
    RotateUV(spin, coin->uvs);
}
