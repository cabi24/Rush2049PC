/* AnimateFlag: the first frame of a waving flag track object (MAT3 targets.c _AnimateFlag). It colours the flag
 * by its name (blue, green, else red), picks the frame of the wave it starts on from its name's "_OFFn", then
 * hands the visual over to AnimateObjSwapGeneric, which waves it through its models from then on. */
#include "game/game.h"

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. For a
 * flag the data is its name, in the track WAD's record. */
enum n64p_Target { n64p_Target_ };
enum n64p_char { n64p_char_ };
typedef struct Target {
    u8 pad00[4];
    u8 flags;               /* 0x04 Flags: TARGET_STARTED */
    u8 pad05[7];
    s32 dlIdx;              /* 0x0C its scene object: the handle, whose low half is the object's index */
    u8 pad10[0x5A - 0x10];
    s16 frames;             /* 0x5A the wave's frame count */
    u8 pad5C[0x6C - 0x5C];
    enum n64p_char data;    /* 0x6C the flag's name (a window address) */
} Target;

/* The environment visual (MAT3 tVisual) in its PC layout: next and func are host pointers, data the target's
 * window address. */
typedef struct Visual {
    void *next;
    s16 index;              /* 0x08 the frame of the wave it is on */
    u8 pad0A[6];
    enum n64p_Target data;  /* 0x10 */
    f32 timeStamp;          /* 0x14 the time to the next frame */
    void (*func)();         /* 0x18 the function called each frame */
} Visual;
#define N64P(x) _Generic((x), enum n64p_Target: (Target *)PC_N64P(x), enum n64p_char: (char *)PC_N64P(x), \
    default: (x))

#define TARGET_STARTED 1        /* the flag has been coloured */
#define FLAG_FRAMES 10          /* the wave's models */
/* PC: retail calls it through an implicit declaration (an int result) with a fifth word; the definition
 * returns the palette record, which an int would cut to 32 bits. Old-style, so the five-word call stays. */
intptr_t MBOX_FindPalette_WORK();

/* The flag's visual function (op 1 each frame, op 0 to release it). The first unpaused frame colours the flag: its
 * palette (flag_pals: red, green, blue) by "BLU" or "GRN" in its name, written into its scene object, and its first
 * frame of the wave from the digit after "_OFF" (0 without one). Every unpaused frame then makes
 * AnimateObjSwapGeneric the visual's function, due at once. */
void AnimateFlag(Visual *v, s16 op) {
    s32 paletteHandle;
    char *name;
    Target *t;
    char *offSuffix;
    s32 i;
    intptr_t palette;

    if (op == 0) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState != 0) {
        return;
    }
    t = N64P(v->data);
    if (!(t->flags & TARGET_STARTED)) {
        name = N64P(t->data);
        t->frames = FLAG_FRAMES;
        if (game_strstr(name, "BLU") != 0) {
            i = 2;
        } else if (game_strstr(name, "GRN") != 0) {
            i = 1;
        } else {
            i = 0;
        }
        palette = MBOX_FindPalette_WORK(flag_pals[i], &paletteHandle, 0, (s8)(gNumModels[0] - 1), 1);
        i = t->dlIdx;
        ObjInstanceList[i].palette = (void *) palette;
        offSuffix = game_strstr(N64P(t->data), "_OFF");
        if (offSuffix != 0) {
            i = (s8)(offSuffix[4] - '0');
        } else {
            i = 0;
        }
        v->index = i;
        t->flags |= TARGET_STARTED;
    }
    v->func = AnimateObjSwapGeneric;
    v->timeStamp = 0.0f;
}
