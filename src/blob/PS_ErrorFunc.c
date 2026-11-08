/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * PS_ErrorFunc (0x800DE20C, 584 B) -- modal settings dialog runner (name is a historical label).
 * Starts a menu sound, draws the dialog frame (SetPakError), saves each viewport's fog
 * parameters (ZOID_GetFog reads ZoidWinInfo[i] +0x40..+0x47) and sets fog off (1000/999,
 * black, alpha 255), then runs ShowPakError(arg0, arg4, arg5, &done, arg6) until `done`,
 * calling game_mode_handler on thread 7 and func_800C813C(0, 0) except for modes 10-13;
 * finally stops the sound and restores every viewport's fog. N64-specific UI (no arcade
 * counterpart). Locals are declared in retail stack order (done at 151, snd at 144, arrays 136..112).
 * No shaping devices. Unit: the whole-program unit score PS_ErrorFunc --neighbours -> EQUAL, 0 locked differ.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;
typedef s32 OSId;
typedef struct OSThread OSThread;

extern s16 gNumWindows;
extern struct pc_multiblit PS_ErrorFunc_dimMultiBlit[1];   /* NewMultiBlit's descriptor (pc_prelude.h) */

extern intptr_t NewMultiBlit(s16 arg0, s16 arg1, void *arg2, s16 arg3);
extern void RemoveBlit(intptr_t sound_id);
extern void UpdateFrame(void);
extern OSId osGetThreadId(OSThread *thread);
s32 ShowPakError(s32 arg0, intptr_t arg1, intptr_t arg2, s8 *done, s32 (*arg4)(void));
void SetPakError(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void ZOID_GetFog(s16 *arg0, s16 *arg1, u8 *arg2, u8 *arg3, u8 *arg4, u8 *arg5, s32 arg6);
void ZOID_SetFog(s16 arg0, s16 arg1, u8 arg2, u8 arg3, u8 arg4, u8 arg5, s32 arg6);
void SaveServer(s32 arg0, s32 arg1);

void PS_ErrorFunc(s32 arg0, s32 mode, s32 arg2, s32 arg3, intptr_t arg4, intptr_t arg5, s32 (*arg6)(void)) {
    s8 done;
    intptr_t snd;
    s16 a[4];
    s16 b[4];
    u8 c[4];
    u8 d[4];
    u8 e[4];
    u8 f[4];
    s32 i;

    done = 0;
    snd = NewMultiBlit(0, 0, &PS_ErrorFunc_dimMultiBlit, 1);
    SetPakError(arg0, mode, 10, 10, 300, 180);
    for (i = 0; i < gNumWindows; i++) {
        ZOID_GetFog(&a[i], &b[i], &c[i], &d[i], &e[i], &f[i], i);
        ZOID_SetFog(1000, 999, 0, 0, 0, 255, i);
    }
    while (!done) {
        if (osGetThreadId(0) == 7) {
            UpdateFrame();
        }
        if (mode != 11 && mode != 12 && mode != 13 && mode != 10) {
            SaveServer(0, 0);
        }
        ShowPakError(arg0, arg4, arg5, &done, arg6);
    }
    RemoveBlit(snd);
    for (i = 0; i < gNumWindows; i++) {
        ZOID_SetFog(a[i], b[i], c[i], d[i], e[i], f[i], i);
    }
}
