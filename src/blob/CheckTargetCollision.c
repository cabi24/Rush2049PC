/* CheckTargetCollision: a car against the track's targets (MAT3 targets.c). */
#include "game/game.h"

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): its position. */
typedef struct Car {
    char pad0[8];
    f32 dr_pos[3];
    char pad14[0x110 - 0x14]; struct pc_visual visuals[21]; char pad14_308[0x380 - 0x308]; void *input; char pad14_384[0x3B8 - 0x384];
} Car;

/* Tests the car in slot `slot` against the targets it may hit, and starts each one it hits (its kind's startFunc,
 * once its Flags have 4 and its slot is the car's): in battle first every target of gAlwaysColList (each leaves the
 * list once hit), then the targets of the quadtree's leaf the car is over (gTargetGroups), those still active with
 * a collision test. The test is the kind's (gTargetCollFuncPtrs): 0-2 a cylinder or radius (the car's slot, the
 * target's position and radius), 3-5 the target against the car. Not in the attract mode's demo without the
 * Expansion Pak. */
void CheckTargetCollision(s16 slot) {
    s32 unusedA[6];
    f32 pos[3];
    s32 unusedB[9];
    Car *car;
    tAlwaysCol *acol; tAlwaysCol *acolnext; tTarget *t; TargInfo *info; tTargetGroup *tg; s32 currentG; s32 i; s32 collFunc; void *d1; void *d2; void *d3; f32 x; f32 z;

    car = PC_CAR(slot);
    if ((gGameState & GS_DEMO) && expansion_pak_present == 0) {
        return;
    }
    if (gRace == kBattle) {
        for (acol = gAlwaysColList.head; acol != 0; acol = acolnext) {
            t = acol->t;
            acolnext = acol->next;
            info = &targetInfo[t->infoIdx];
            if (gTargetCollFuncPtrs[info->collFuncIdx](&slot, t->pos, &t->radius, 0)) {
                t->Flags |= 4;
                t->slot = slot;
                if (info->startFunc != 0) {
                    ((void (*)(tTarget *)) info->startFunc)(t);
                }
                FreeStatLLE(&gAlwaysColList, acol);
            }
        }
    }
    pos[0] = car->dr_pos[0];
    pos[1] = car->dr_pos[1];
    pos[2] = car->dr_pos[2];
    /* down the quadtree to the leaf under the car; off its edge, nothing */
    currentG = 0;
    for (;;) {
        tg = &gTargetGroups[currentG];
        if (tg->minh <= pos[0] && pos[0] < tg->maxh && tg->minv <= pos[2] && pos[2] < tg->maxv) {
            if (tg->flags & 1) {
                z = gTargetGroups[tg->child[2]].maxv;
                x = gTargetGroups[tg->child[2]].maxh;
                if (z <= pos[2]) {
                    if (pos[0] < x) {
                        currentG = tg->child[0];
                    } else {
                        currentG = tg->child[1];
                    }
                } else {
                    if (pos[0] < x) {
                        currentG = tg->child[2];
                    } else {
                        currentG = tg->child[3];
                    }
                }
            } else {
                break;
            }
        } else {
            return;
        }
    }
    for (i = 0; i < tg->child[0]; i++) {
        t = gTargetGroupList[tg->child[1] + i].target;
        if (t != 0 && (t->Flags & TARGET_ACTIVE) && (t->Flags & TARGET_COLLIDES)) {
            info = &targetInfo[t->infoIdx];
            collFunc = info->collFuncIdx;
            switch (collFunc) {
                case 0:
                case 1:
                case 2:
                    d1 = &slot;
                    d2 = t->pos;
                    d3 = &t->radius;
                    break;
                case 3:
                case 4:
                case 5:
                    d1 = t;
                    d2 = &slot;
                    d3 = 0;
                    break;
                default:
                    continue;
            }
            if (gTargetCollFuncPtrs[collFunc](d1, d2, d3, 0)) {
                t->Flags |= 4;
                t->slot = slot;
                if (info->startFunc != 0) {
                    ((void (*)(tTarget *)) info->startFunc)(t);
                }
            }
        }
    }
}
