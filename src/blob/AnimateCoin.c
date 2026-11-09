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

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
enum n64p_CoinData { n64p_CoinData_ };
enum n64p_Target { n64p_Target_ };
typedef struct Target {
    u8 pad00[4];
    u8 flags;               /* 0x04 Flags: TARGET_STARTED, TARGET_GOLD, TARGET_COIN_GONE */
    u8 pad05[7];
    SceneHandle dlIdx;      /* 0x0C the coin's glow, a scene object */
    u8 pad10[0x38 - 0x10];
    f32 matPos[3];          /* 0x38 mat: its position */
    u8 pad44[0x6C - 0x44];
    enum n64p_CoinData data; /* 0x6C its CoinData (a window address) */
} Target;

/* The environment visual (MAT3 tVisual) in its PC layout: next is a host pointer, data the target's window address. */
typedef struct Visual {
    void *next;
    u8 pad08[8];
    enum n64p_Target data;  /* 0x10 */
} Visual;
#define N64P(x) _Generic((x), enum n64p_CoinData: (CoinData *)PC_N64P(x), enum n64p_Target: (Target *)PC_N64P(x), \
    default: (x))

#define TARGET_STARTED 1        /* the coin has been set up */
#define TARGET_GOLD 0x40        /* a gold coin (else silver) */
#define TARGET_COIN_GONE 0x80   /* every player has found it */

/* A player's window bit; a view of -1 shifts by a negative count, undefined in C. The N64's sllv, x86-64's shl and
 * aarch64's lsl all take the count's low 5 bits, so this does (docs/port/SANITIZE.md). */
#define VIEW_BIT(s) 1 << ((s) & 31)

VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(s32, gTargetList);
#define gTargetList gTargetList_view

extern s32 ZOID_AddObject(s32, void *, s16, s32);

/* One frame of a coin (op 1), or release its visual (op 0). Gold coins are numbered from 8 (gWhichGoldCoin + 8),
 * silver ones from 0, in the order their visuals first run; a coin every player has already found is removed at
 * once. Nothing moves while the game is paused. */
void AnimateCoin(Visual *v, s16 op)
{
    Target *t;
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
    t = N64P(v->data);
    coin = N64P(t->data);
    if (t->flags & TARGET_COIN_GONE) {
        RemoveObjectTree(coin->dlIdx.part.index, 0, 0);
        RemoveObjectTree(t->dlIdx.part.index, 0, 0);
        FreeStatLLE(&gTargetList, t);
        ReleaseEnviron(v, 1);
        return;
    }
    if (!(t->flags & TARGET_STARTED)) {
        goldGlow[0] = PC_RGBA(coin_gold_glow_color);
        silverGlow[0] = PC_RGBA(coin_silver_glow_color);
        if (t->flags & TARGET_GOLD) {
            number = gWhichGoldCoin + 8;
            coin->whichcoin = number;
            gWhichGoldCoin++;
        } else {
            number = gWhichSilverCoin;
            coin->whichcoin = number;
            gWhichSilverCoin++;
        }
        ZOID_SetObjectDef(t->dlIdx.part.index, MBOX_FindObject_WORK(coin_glow_object_name, 0, gNumModels - 1, 1));
        coin->angV[0] = 0.0f;
        coin->angV[2] = 0.0f;
        coin->angV[1] = 3.0f;
        CopyUV(unitmatrix, coin->uvs);
        coin->pos[0] = t->matPos[0];
        coin->pos[1] = t->matPos[1];
        coin->pos[2] = t->matPos[2];
        if (t->flags & TARGET_GOLD) {
            coin->dlIdx.handle = ZOID_AddObject(MBOX_FindObject_WORK(gold_coin_object_name, 0, gNumModels - 1, 1), coin->uvs, -1, 0x40000);
            ZOID_SetColor(t->dlIdx.part.index, goldGlow);
        } else {
            coin->dlIdx.handle = ZOID_AddObject(MBOX_FindObject_WORK(silver_coin_object_name, 0, gNumModels - 1, 1), coin->uvs, -1, 0x40000);
            ZOID_SetColor(t->dlIdx.part.index, silverGlow);
        }
        coin->cameras = 0;
        foundByAll = 1;
        for (i = 0; i < gNumPlayers; i++) {
            if (!IsCoinFound(i, number)) {
                foundByAll = 0;
                coin->cameras |= 1 << i;
            } else {
                car = PC_CAR(i);
                MBOX_HideObject(t->dlIdx.handle, 1, VIEW_BIT(car->view));
                MBOX_HideObject(coin->dlIdx.handle, 1, VIEW_BIT(car->view));
            }
        }
        if (foundByAll) {
            RemoveObjectTree(coin->dlIdx.part.index, 0, 0);
            RemoveObjectTree(t->dlIdx.part.index, 0, 0);
            FreeStatLLE(&gTargetList, t);
            ReleaseEnviron(v, 1);
            return;
        }
        t->flags |= TARGET_STARTED;
    }
    angV = N64P(t->data)->angV;
    spin[0] = angV[0] * gFrameSeconds;
    spin[1] = angV[1] * gFrameSeconds;
    spin[2] = angV[2] * gFrameSeconds;
    RotateUV(spin, coin->uvs);
}
