/* BattleDamage: a car damages another in battle (MAT3 shots.c). */
#include "game/game.h"

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its slot and battle state (battle.health,
 * damageThisFrame, defense.active). */
typedef struct Player { u8 unknown000[0x110 - 0x0]; struct pc_visual visuals[21]; u8 unknown000_308[0x35B - 0x308]; s8 slot; u8 unknown35c[0x380 - 0x35C]; void *input; u8 unknown35c_384[0x386 - 0x384]; s16 health; s16 damageThisFrame; u8 unknown38a[2]; u32 defenseActive; u8 unknown390[0x28]; } Player;

/* Car `attacker` does `damage` to car `victim` (not itself, a team-mate or a crashed car); a shielded victim
 * (defence 4) takes a fifth. At no health left the victim crashes and the attacker scores a point. */
void BattleDamage(Player *attacker, Player *victim, s32 damage)
{
    if (PC_MODEL_CRASHFLAG(victim->slot)) return;
    if (victim->slot == attacker->slot) return;
    if (gCarTeam[victim->slot] == gCarTeam[attacker->slot]) return;
    if (victim->defenseActive & 4) damage = damage * 0.2f;
    victim->damageThisFrame = damage;
    victim->health -= damage;
    if (victim->health <= 0) {
        victim->health = 0;
        AddBattlePoint(attacker->slot, victim->slot, 1);
        PC_MODEL_CRASHFLAG(victim->slot) = 1;
    }
}
