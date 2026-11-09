#include "game/game.h"

typedef struct Player { u8 unknown000[0x110 - 0x0]; struct pc_visual visuals[21]; u8 unknown000_308[0x35B - 0x308]; s8 index; u8 unknown35c[0x380 - 0x35C]; void *input; u8 unknown35c_384[0x386 - 0x384]; s16 health; s16 damage; u8 unknown38a[2]; u32 flags; u8 unknown390[0x28]; } Player;

void BattleDamage(Player *attacker, Player *victim, s32 damage)
{
    if (PC_MODEL_CRASHFLAG(victim->index)) return;
    if (victim->index == attacker->index) return;
    if (gCarTeam[victim->index] == gCarTeam[attacker->index]) return;
    if (victim->flags & 4) damage = damage * 0.2f;
    victim->damage = damage;
    victim->health -= damage;
    if (victim->health <= 0) {
        victim->health = 0;
        AddBattlePoint(attacker->index, victim->index, 1);
        PC_MODEL_CRASHFLAG(victim->index) = 1;
    }
}
