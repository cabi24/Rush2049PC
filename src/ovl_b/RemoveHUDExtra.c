/* RemoveHUDExtra: remove the battle and stunt HUD extras (MAT3 hudextra.c; runtime image B). */
#include "game/game.h"
#define tHudIcon tHudIconView   /* this file's own tHudIcon: include/game's tHudIcon is another type */
#define tStuntCoin tStuntCoinView   /* this file's own tStuntCoin: include/game's tStuntCoin is another type */

typedef struct tUVMatrix { f32 uvs[4][3]; } tUVMatrix;
typedef struct tHudIcon { s16 type, idx; tUVMatrix mat; } tHudIcon;          /* anon_type_idx_34 */
typedef struct tBattleCoin { s16 idx; s16 pad; tUVMatrix mat; } tBattleCoin; /* anon_idx_mat_34 */
typedef struct tStuntCoin {
    s16 type, idx;
    tUVMatrix mat;
    f32 alpha;
    s16 screenPos[2], startPos[2], destPos[2];
    f32 scrollTime;
} tStuntCoin;

VIEW(tHudIcon[4], weaponIcon);
#define weaponIcon weaponIcon_view
VIEW(tHudIcon[4], powerupIcon);
#define powerupIcon powerupIcon_view
VIEW(tBattleCoin[4], battleCoin);
#define battleCoin battleCoin_view
VIEW(tStuntCoin[4][10], stuntCoin);
#define stuntCoin stuntCoin_view

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
