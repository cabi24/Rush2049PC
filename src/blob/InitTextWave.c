/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef int s32;
extern s8 text_wave_inited;
extern s16 gLang;
extern u8 gLargestTextWave,gTextWave;
extern s32 gTextWaveInst;
extern s32 LoadWave(s32 kind, s32 skip, s32 async, s32 flag, void *buffer);
extern void LockWaveInstance(s32),AdjustTextWavePointers(void),LoadTextWave(void);
void InitTextWave(void)
{
    const s8 initialized = 1;

    if (text_wave_inited == 0) {
        text_wave_inited = initialized;
        gLang = 0;
        gTextWave = gLargestTextWave;
        gTextWaveInst = LoadWave(gLargestTextWave, 0, 0, 0, 0);
        LockWaveInstance(gTextWaveInst);
        AdjustTextWavePointers();
        LoadTextWave();
    }
}
