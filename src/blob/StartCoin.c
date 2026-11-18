/* StartCoin: the start function of the gold and silver coins, the stunt and obstacle tracks' collectables (MAT3
 * targets.c _StartCoin). It runs when a car touches a coin (CheckTargetCollision through targetInfo[].start). */
#include "game/game.h"

/* A car has touched the coin (the target t; its dlIdx is the coin's glow, its data the coin's record): if the car is
 * a player's (it has a window) and the player hasn't found this coin, the profile records it, the coin disappears
 * from that player's window, and when no window shows it any more the coin is marked gone. The hit is handled either
 * way; the coin stays active for the other players. */
void StartCoin(tTarget *t)
{
    s8 slot;
    tTargetStructCoin *coin;

    slot = t->slot;
    t->Flags = (u8) (t->Flags & ~TARGET_HIT);
    coin = t->data;
    if ((pc_cars[slot].camera >= 0) && (IsCoinFound(slot, coin->whichcoin) == 0)) {
        FoundCoin(t->slot, coin->whichcoin);
        MBOX_HideObject(t->dlIdx, 1, 1 << pc_cars[t->slot].camera);
        MBOX_HideObject(coin->dlIdx, 1, 1 << pc_cars[t->slot].camera);
        if ((coin->cameras &= ~(1 << pc_cars[t->slot].camera)) == 0) {
            t->Flags = (u8) (t->Flags | TARGET_COIN_GONE);
        }
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
    }
}
