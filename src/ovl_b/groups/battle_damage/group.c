/* BattleDamage: a car damages another in battle (MAT3 shots.c). */
#include "game/game.h"

/* Car `attacker` does `damage` to car `victim` (not itself, a team-mate or a crashed car); a shielded victim
 * (defence 4) takes a fifth. At no health left the victim crashes and the attacker scores a point. */
void BattleDamage(tCarData *attacker, tCarData *victim, s32 damage)
{
    if (pc_models[victim->slot].crashflag) return;
    if (victim->slot == attacker->slot) return;
    if (gCarTeam[victim->slot] == gCarTeam[attacker->slot]) return;
    if (victim->battle.defense.active & 4) damage = damage * 0.2f;
    victim->battle.damageThisFrame = damage;
    victim->battle.health -= damage;
    if (victim->battle.health <= 0) {
        victim->battle.health = 0;
        AddBattlePoint(attacker->slot, victim->slot, 1);
        pc_models[victim->slot].crashflag = 1;
    }
}
