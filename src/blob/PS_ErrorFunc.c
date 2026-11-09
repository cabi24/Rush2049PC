#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;

VIEW(struct pc_multiblit[1], PS_ErrorFunc_dimMultiBlit);
#define PS_ErrorFunc_dimMultiBlit PS_ErrorFunc_dimMultiBlit_view

extern intptr_t NewMultiBlit(s16 arg0, s16 arg1, void *arg2, s16 arg3);
extern void RemoveBlit(intptr_t sound_id);

extern OSId osGetThreadId(OSThread *thread);
s32 ShowPakError(s32 arg0, intptr_t arg1, intptr_t arg2, s8 *done, s32 (*arg4)(void));

void ZOID_SetFog(s16 arg0, s16 arg1, u8 arg2, u8 arg3, u8 arg4, u8 arg5, s32 arg6);

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
