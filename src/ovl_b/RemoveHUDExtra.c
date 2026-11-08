/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image B, 0x8039244C..0x803925C8 (380 bytes): RemoveHUDExtra. Removes the
 * stunt/battle HUD multi-blit and every player's weapon icon, power-up icon,
 * battle coin and ten stunt coins (zoid objects), resetting their kinds.
 * Donor: MAT3 GameCube decomp, GameSource/src/hudextra.c RemoveHUDExtra
 * (GC 0x80022934, size 0x10c); direct typed port, MAT3 names and statement
 * order, N64 offsets (icon records 0x34 bytes, stunt coin 0x48 bytes).
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * credited for the record layouts and callee identities). Callee names in the
 * repo are historical: RemoveBlit, ZOID_RemoveObject =
 * ZOID_RemoveObject (s16 object index). No shaping, no unused locals.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;

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

extern void *hudextra_blit;
extern s16 gNumWindows;
extern tHudIcon weaponIcon[4], powerupIcon[4];
extern tBattleCoin battleCoin[4];
extern tStuntCoin stuntCoin[4][10];
extern void RemoveBlit(void *);
extern void ZOID_RemoveObject(s16);
#define blit hudextra_blit

void RemoveHUDExtra(void) /* RemoveHUDExtra */
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
