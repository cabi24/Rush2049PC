/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;
extern s8 gBattlePowerUps;

void IncBattlePowerUps(void)
{
    gBattlePowerUps++;
}
