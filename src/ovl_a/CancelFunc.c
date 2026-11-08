/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef int s32;
s32 PS_PakChanged(s32 arg0);
s32 PS_PakDamaged(s32 arg0);
extern s32 gPakErr;

s32 CancelFunc(s32 arg0)
{
    if (PS_PakChanged(arg0) || (gPakErr == 2 && PS_PakDamaged(arg0))) {
        return 1;
    }
    return 0;
}
