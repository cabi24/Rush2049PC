/* IPA group LockAndLoad, file 2 of 2.
 * LockAndLoad takes its argument in $t0 (IDO -O3 interprocedural
 * register allocation) and keeps it live across FindWaveInstanceInternal, whose
 * temporaries are therefore confined to $t6-$t9. other_user is a stand-in for
 * the real second caller (uninitGameState).
 */
typedef signed char s8; typedef unsigned char u8; typedef int s32;
typedef struct { u8 pad0[5]; s8 sub; u8 id; u8 pad7[5]; void *ptr; u8 pad10[4]; } Voice;
extern Voice gWaveInstance[64];
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
extern s32 LoadWave(s32 a, s32 b, s32 c, s32 d, void *e);
extern void LockWaveInstance(s32 a);

extern s32 FindWaveInstanceInternal(s32 arg0, s8 arg1);
void LockAndLoad(s32 id)
{
  if (FindWaveInstanceInternal(id, -1) < 0)
  {
    LockWaveInstance(LoadWave(id, 0, 0, 0, 0));
  }
}

void LoadFrontEndWaves(void)
{
  LockAndLoad(54);
  LockAndLoad(58);
  LockAndLoad(59);
}

void other_user(s32 x)
{
  LockAndLoad(x);
}

