/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* ReplaceWave = ReplaceWave (MAT3 GameSource/src/waves.c ReplaceWave, GC 0x8005B6EC) with
 * _ReplaceWaveInstance (GC 0x8005BAC4) inlined.
 * Author: (unchanged code). packaging: the inlined
 * helper is non-static, named after its retail deleted stub ReplaceWaveInstance, so the stub carries a symbol. */
typedef signed char s8;
typedef signed short s16;
typedef struct anon_loaded_ready_14 {
    signed char loaded;
    signed char ready;
    signed char locked;
    signed char model;
    unsigned char mirrored;
    signed char handle;
    unsigned char wave;
    unsigned int size;
    unsigned char *ptr;
    unsigned char *lastPtr;
} anon_loaded_ready_14;

extern anon_loaded_ready_14 D_80156D38[64];  /* gWaveInstance */
extern s16 gRenderInProgress;                      /* gRenderInProgress */
#define gWaveInstance D_80156D38

int FindWaveInstanceInternal(int waveNum, signed char handle);           /* _FindWaveInstance */
void ValidateWaveInstance(int idx, int loaded, int ready);           /* _ValidateWaveInstance */
void SetWaveInstanceModel(int idx);                                  /* _SetWaveInstanceModel */
void ReadWave(int idx);                                  /* _ReadWave */

void ReplaceWaveInstance(int idx, int newWave)  /* _ReplaceWaveInstance (deleted stub 0x800A5014) */
{
    anon_loaded_ready_14 *winst;
    winst = &gWaveInstance[idx];
    ValidateWaveInstance(idx, 1, 1);
    winst->loaded = 0; winst->ready = 0; winst->wave = newWave; winst->mirrored = 0;
    SetWaveInstanceModel(idx);
}

void ReplaceWave(int curWave, int newWave)
{
    int idx;
    if (FindWaveInstanceInternal(newWave, -1) < 0) {
        idx = FindWaveInstanceInternal(curWave, -1);
        if (idx >= 0) {
            while (*(volatile s16 *)&gRenderInProgress) { pc_os_spin_yield("ReplaceWave"); }
            ReplaceWaveInstance(idx, newWave); ReadWave(idx);
        }
    }
}
