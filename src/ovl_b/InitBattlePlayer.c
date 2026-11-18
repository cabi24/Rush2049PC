/* InitBattlePlayer: a car's battle state at a battle's start (MAT3 weapons.c). */
#include "game/game.h"

/* Resets car `slot`'s battle state: no defence, not fading, not shooting, unloaded, no weapon and no ammunition, the
 * weapon shown 9 (none), and its cannon and machine gun flashes off. */
void InitBattlePlayer(s16 slot)
{
    tCarData *gc;
    s32 i;

    gc = PC_CAR(slot);
    gc->battle.defense.active = 0;
    gc->battle.fadeState = 0;
    gc->battle.shoot = 0;
    gc->battle.loaded = 0;
    gc->battle.weapon.type = 8;
    gc->battle.weapon.ammoCnt = -1;
    gCurWeaponType[slot] = 9;
    gCannMuzzleObjs[slot].ripdl = -1;
    gCannMuzzleObjs[slot].ripCnt = -1;
    gCannMuzzleObjs[slot].onTime = 0.0f;
    gGattMuzzleObjs[slot].onTime = 0.0f;
    for (i = 0; i < 5; i++) {
        gGattMuzzleObjs[slot].mdls[i] = -1;
    }
}
