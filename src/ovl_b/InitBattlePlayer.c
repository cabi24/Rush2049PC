/* InitBattlePlayer: a car's battle state at a battle's start (MAT3 weapons.c). */
#include "game/game.h"

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its battle state (MAT3 battle). */
typedef struct CarBattleView {
    unsigned char unknown000[0x110 - 0x0]; struct pc_visual visuals[21]; unsigned char unknown000_308[0x380 - 0x308]; void *input;
    s8 weaponType;      /* 0x384 battle.weapon.type: 8 none */
    s8 ammoCnt;         /* 0x385 battle.weapon.ammoCnt */
    unsigned char unknown386[6];
    s32 defenseActive;  /* 0x38C battle.defense.active */
    unsigned char unknown390[0x10];
    s8 shoot;           /* 0x3A0 battle.shoot */
    unsigned char unknown3a1;
    s8 fadeState;       /* 0x3A2 battle.fadeState */
    unsigned char unknown3a3;
    s32 loaded;         /* 0x3A4 battle.loaded */
    unsigned char unknown3a8[0x10];
} CarBattleView;

/* Resets car `slot`'s battle state: no defence, not fading, not shooting, unloaded, no weapon and no ammunition, the
 * weapon shown 9 (none), and its cannon and machine gun flashes off. */
void InitBattlePlayer(s16 slot)
{
    CarBattleView *gc;
    s32 i;

    gc = PC_CAR(slot);
    gc->defenseActive = 0;
    gc->fadeState = 0;
    gc->shoot = 0;
    gc->loaded = 0;
    gc->weaponType = 8;
    gc->ammoCnt = -1;
    gCurWeaponType[slot] = 9;
    gCannMuzzleObjs[slot].ripdl = -1;
    gCannMuzzleObjs[slot].ripCnt = -1;
    gCannMuzzleObjs[slot].onTime = 0.0f;
    gGattMuzzleObjs[slot].onTime = 0.0f;
    for (i = 0; i < 5; i++) {
        gGattMuzzleObjs[slot].mdls[i] = -1;
    }
}
