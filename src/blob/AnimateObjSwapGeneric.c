/* AnimateObjSwapGeneric: a track object that cycles through models, a visual's function (MAT3 visuals.c). */
#include "game/game.h"

#define TARGET_REMOVE_AT_END 0x1000   /* targetInfo flags: the cycle runs once, then the visual ends */
#define TARGET_DESTROY_AT_END 0x2000  /* the cycle runs once, then the object goes */
#define SWAP_FRAME_TIME 0.0625f       /* seconds a frame */

/* Every SWAP_FRAME_TIME seconds (not while paused) shows the next of the target's models (its otype .. otype +
 * frames - 1 of gObjList) on its scene object (its dlIdx, whose low half is the object's index) and keeps the one it
 * shows as its type; at the end of the cycle starts again, or ends the visual (and with TARGET_DESTROY_AT_END
 * removes the object). mode 0 ends the visual at once. The visual (obj) holds the target in its data, the frame it
 * shows in its index and the time to the next in its timeStamp. */
void AnimateObjSwapGeneric(struct pc_visual *obj, s16 mode)
{
    tTarget *t;
    TargInfo *info;
    s16 f;
    s16 id;
    u16 tex;

    if (mode == 0) {
apply:
        ReleaseEnviron(obj, 1);
        return;
    }
    if (gPauseState != 0) {
        return;
    }
    t = (tTarget *) obj->data;
    obj->timeStamp -= gFrameSeconds;
    if (obj->timeStamp > 0.0f) {
        return;
    }
    obj->index++;
    obj->timeStamp = SWAP_FRAME_TIME;
    if (obj->index >= t->frames) {
        info = &targetInfo[t->infoIdx];
        if (info->Flags & TARGET_REMOVE_AT_END) {
            goto apply;
        }
        if (info->Flags & TARGET_DESTROY_AT_END) {
            RemoveObjectTree(t->dlIdx, 0, 0);
            goto apply;
        }
        obj->index = 0;
    }
    f = t->otype + obj->index;
    if (f != t->type) {
        if (gObjList[f]) {}
        id = t->dlIdx;
        tex = gObjList[f];
        ObjInstanceList[id].index = tex;
        t->type = f;
    }
}
