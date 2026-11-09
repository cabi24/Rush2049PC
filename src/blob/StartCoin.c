/* StartCoin: the start function of the gold and silver coins, the stunt and obstacle tracks' collectables (MAT3
 * targets.c _StartCoin). It runs when a car touches a coin (CheckTargetCollision through targetInfo[].start). */
#include "game/game.h"

/* A coin's own record (MAT3 tTargetStructCoin, 0x4C bytes), the target's per-kind data. Only the fields read here. */
typedef struct CoinData {
    s32 dlIdx;              /* 0x00 the spinning coin's scene object */
    u8 pad04[0x48 - 0x04];
    u16 cameras;            /* 0x48 a bit per window that still shows the coin */
    u16 whichcoin;          /* 0x4A the coin's number in the profile's found-coin bits (gold coins from 8) */
} CoinData;

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
typedef struct Target {
    u8 pad00[4];
    u8 flags;               /* 0x04 Flags: TARGET_HIT, TARGET_COIN_GONE */
    u8 pad05[7];
    s32 dlIdx;              /* 0x0C the coin's glow, a scene object */
    s16 infoIdx;            /* 0x10 its kind: the index of its targetInfo record */
    u8 pad12[0x38 - 0x12];
    f32 matPos[3];          /* 0x38 its position, where its sounds play */
    u8 pad44[0x5C - 0x44];
    s8 slot;                /* 0x5C the car that touched it */
    u8 pad5D[0x6C - 0x5D];
    u32 data;               /* 0x6C its CoinData (a window address) */
} Target;

/* A car has touched the coin: if the car is a player's (it has a window) and the player hasn't found this coin, the
 * profile records it, the coin disappears from that player's window, and when no window shows it any more the coin
 * is marked gone. The hit is handled either way; the coin stays active for the other players. */
void StartCoin(Target *t)
{
    s8 slot;
    CoinData *coin;

    slot = t->slot;
    t->flags = (u8) (t->flags & ~TARGET_HIT);
    coin = (void *) PC_N64P(t->data);
    if ((PC_CAR_CAMERA(slot) >= 0) && (IsCoinFound(slot, coin->whichcoin) == 0)) {
        FoundCoin(t->slot, coin->whichcoin);
        MBOX_HideObject(t->dlIdx, 1, 1 << PC_CAR_CAMERA(t->slot));
        MBOX_HideObject(coin->dlIdx, 1, 1 << PC_CAR_CAMERA(t->slot));
        if ((coin->cameras &= ~(1 << PC_CAR_CAMERA(t->slot))) == 0) {
            t->flags = (u8) (t->flags | TARGET_COIN_GONE);
        }
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->matPos, 2);
    }
}
