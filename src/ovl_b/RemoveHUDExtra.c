/* RemoveHUDExtra: remove the battle and stunt HUD extras (MAT3 hudextra.c; runtime image B). */
#include "game/game.h"
extern void RemoveBlit(void *);

#define blit hudextra_blit

/* Removes the extras' multiblit and every window's weapon, powerup, battle coin and stunt coin icons (3D objects),
 * leaving them their "none" types (8, 0 and 11). */
void RemoveHUDExtra(void)
{
    int player, j;
    if (blit) { RemoveBlit(blit); blit = 0; }
    for (player = 0; player < gNumWindows; player++) {
        if (weaponIcon[player].idx != -1) {
            ZOID_RemoveObject(weaponIcon[player].idx);
            weaponIcon[player].idx = -1;
        }
        weaponIcon[player].type = 8;
        if (powerupIcon[player].idx != -1) {
            ZOID_RemoveObject(powerupIcon[player].idx);
            powerupIcon[player].idx = -1;
        }
        powerupIcon[player].type = 0;
        if (battleCoin[player].idx != -1) {
            ZOID_RemoveObject(battleCoin[player].idx);
            battleCoin[player].idx = -1;
        }
        for (j = 0; j < 10; j++) {
            if (stuntCoin[player][j].idx != -1) {
                ZOID_RemoveObject(stuntCoin[player][j].idx);
                stuntCoin[player][j].idx = -1;
            }
            stuntCoin[player][j].type = 11;
        }
    }
}
