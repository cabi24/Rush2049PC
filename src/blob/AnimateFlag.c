/* AnimateFlag: the first frame of a waving flag track object (MAT3 targets.c _AnimateFlag). It colours the flag
 * by its name (blue, green, else red), picks the frame of the wave it starts on from its name's "_OFFn", then
 * hands the visual over to AnimateObjSwapGeneric, which waves it through its models from then on. */
#include "game/game.h"


#define FLAG_FRAMES 10          /* the wave's models */
/* The flag's visual function (op 1 each frame, op 0 to release it). The first unpaused frame colours the flag: its
 * palette (flag_pals: red, green, blue) by "BLU" or "GRN" in its name, written into its scene object, and its first
 * frame of the wave from the digit after "_OFF" (0 without one): the visual's index. Every unpaused frame then
 * makes AnimateObjSwapGeneric the visual's function, due at once. The visual's data is the flag's target, whose
 * data is its world object, which starts with its name (a track WAD record, SetupTarget). */
void AnimateFlag(struct pc_visual *v, s16 op) {
    u16 paletteHandle;
    char *name;
    tTarget *t;
    char *offSuffix;
    s32 i;
    tPaletteDef *palette;

    if (op == 0) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState != 0) {
        return;
    }
    t = (tTarget *) v->data;
    if (!(t->Flags & TARGET_SET_UP)) {
        name = t->data;
        t->frames = FLAG_FRAMES;
        if (game_strstr(name, "BLU") != 0) {
            i = 2;
        } else if (game_strstr(name, "GRN") != 0) {
            i = 1;
        } else {
            i = 0;
        }
        palette = MBOX_FindPalette_WORK(flag_pals[i], &paletteHandle, 0, (s8)(gNumModels[0] - 1));
        i = t->dlIdx;
        ObjInstanceList[i].palette = palette;
        offSuffix = game_strstr(t->data, "_OFF");
        if (offSuffix != 0) {
            i = (s8)(offSuffix[4] - '0');
        } else {
            i = 0;
        }
        v->index = i;
        t->Flags |= TARGET_SET_UP;
    }
    v->func = (void (*)(void *, short)) AnimateObjSwapGeneric;
    v->timeStamp = 0.0f;
}
