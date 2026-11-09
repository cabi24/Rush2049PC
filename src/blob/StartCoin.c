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

/* A car has touched the coin (the target t; its dlIdx is the coin's glow, its data the coin's record): if the car is
 * a player's (it has a window) and the player hasn't found this coin, the profile records it, the coin disappears
 * from that player's window, and when no window shows it any more the coin is marked gone. The hit is handled either
 * way; the coin stays active for the other players. */
void StartCoin(tTarget *t)
{
    s8 slot;
    CoinData *coin;

    slot = t->slot;
    t->Flags = (u8) (t->Flags & ~TARGET_HIT);
    coin = t->data;
    if ((PC_CAR_CAMERA(slot) >= 0) && (IsCoinFound(slot, coin->whichcoin) == 0)) {
        FoundCoin(t->slot, coin->whichcoin);
        MBOX_HideObject(t->dlIdx, 1, 1 << PC_CAR_CAMERA(t->slot));
        MBOX_HideObject(coin->dlIdx, 1, 1 << PC_CAR_CAMERA(t->slot));
        if ((coin->cameras &= ~(1 << PC_CAR_CAMERA(t->slot))) == 0) {
            t->Flags = (u8) (t->Flags | TARGET_COIN_GONE);
        }
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
    }
}
