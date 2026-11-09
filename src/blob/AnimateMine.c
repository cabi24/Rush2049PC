#include "game/game.h"

typedef union Color {struct {u8 r,g,b,a;} c;u32 word;} Color;
typedef struct MineData {
 s32 base,ring[2]; f32 timer; f32 mat[2][4][3];
 f32 elapsed[2]; s8 step[2]; u8 alpha[2]; f32 scale[2]; s16 phase,active;
} MineData;
/* PC: a target (a level-object node, 112 bytes) keeps its N64 layout: next and its state, the mine's data (a heap
 * record of its category), are 32-bit window addresses (docs/port/POINTERS.md 10). */
enum n64p_Target { n64p_Target_ };
enum n64p_MineData { n64p_MineData_ };
typedef struct Target {
 enum n64p_Target next;u8 flags,pad5[7];s32 model;s16 info;u8 pad18[2];
 f32 mat[4][3],position[3];s16 kind;u8 pad82[10];s8 car;u8 pad93[3];
 s32 sound;s8 sound_state,owner;u8 pad102[2];f32 age;enum n64p_MineData data;
} Target;
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Visual {struct Visual *next;s16 index,model,owner;u8 pad10[2];enum n64p_Target data;f32 time;void (*callback)(struct Visual*,s16);} Visual;
#define N64P(x) _Generic((x), enum n64p_Target: (Target *)PC_N64P(x), \
    enum n64p_MineData: (MineData *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

typedef struct Texture36 {u8 bytes[36];} Texture36;
typedef struct TextureBank {Texture36 *base;u32 other;} TextureBank;

typedef struct Link {struct Link *next;Target *target;} Link;
/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct Pool {u8 doubly;s32 count,size;void *mem;Link *active,*free;} Pool;

VIEW(s32[10], gWprObjList);
#define gWprObjList gWprObjList_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

VIEW(TextureBank[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view

VIEW(Pool, gAlwaysColList);
#define gAlwaysColList gAlwaysColList_view
VIEW(Pool, gTargetList);
#define gTargetList gTargetList_view

extern void *RoadPosition(f32 *,f32 *,f32 (*)[3]);

extern void ZOID_SetTexture(s16,Texture36 *,s32);

void AnimateMine(Visual *v, s16 op)
{
    int i;
    int j;
    int k;
    Target *t;
    MineData *m;
    Color clr;
    Color *clrPtr;
    f32 opos[3];
    f32 uvs[3][3];
    f32 oldT;
    Link *acol;
    Link *acolnext;
    if (!op) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState != 0) return;
    t = N64P(v->data);
    m = N64P(t->data);
    if (!(t->flags & 1)) {
        if (gPlayerMines[t->owner].count == 3) {
            oldT = 0.0f;
            j = -1;
            for (i = 0; i < 4; i++) {
                if (gPlayerMines[t->owner].target[i] && gPlayerMines[t->owner].target[i]->age > oldT) {
                    j = i;
                    oldT = gPlayerMines[t->owner].target[i]->age;
                }
            }
            if (j != -1) gPlayerMines[t->owner].target[j]->age = 121.0f;
        }
        for (i = 0; i < 4; i++) {
            if (!gPlayerMines[t->owner].target[i]) {
                gPlayerMines[t->owner].target[i] = t;
                gPlayerMines[t->owner].count++;
                break;
            }
        }
        if (RoadPosition(t->position, opos, uvs)) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) t->mat[i][j] = uvs[i][j];
            }
        }
        j = -1;
        m->base = MBOX_NewObject(gWprObjList[5], 0, t->model, 0x2080);
        clr.c.r = 255;
        clr.c.g = 255;
        clr.c.b = 255;
        clr.c.a = 255;
        ZOID_SetColor(m->base, &clr);
        ZOID_SetObjectFlags(t->model, ZOID_ObjectFlags(t->model) | 0x2000);
        ZOID_SetColor(t->model, &clr);
        ZOID_SetTexture(m->base, &gTexDefPtrs[gMineTextures[4] >> 10].base[gMineTextures[4] & 1023], -1);
        m->timer = 0.0f;
        m->phase = 0;
        clrPtr = &clr;
        clrPtr->c.r = gMineRingColors[gCarTeam[t->owner]][0];
        clrPtr->c.g = gMineRingColors[gCarTeam[t->owner]][1];
        clrPtr->c.b = gMineRingColors[gCarTeam[t->owner]][2];
        CopyUV(t->mat, m->mat[0]);
        CopyUV(t->mat, m->mat[1]);
        m->mat[0][3][0] = t->position[0];
        m->mat[0][3][1] = t->position[1];
        m->mat[0][3][2] = t->position[2];
        m->mat[0][3][0] = m->mat[0][3][0] + m->mat[0][1][0] * 0.75f;
        m->mat[0][3][1] = m->mat[0][3][1] + m->mat[0][1][1] * 0.75f;
        m->mat[0][3][2] = m->mat[0][3][2] + m->mat[0][1][2] * 0.75f;
        m->mat[1][3][0] = t->position[0];
        m->mat[1][3][1] = t->position[1];
        m->mat[1][3][2] = t->position[2];
        m->mat[1][3][0] = m->mat[1][3][0] + m->mat[1][1][0];
        m->mat[1][3][1] = m->mat[1][3][1] + m->mat[1][1][1]; m->mat[1][3][2] = m->mat[1][3][2] + m->mat[1][1][2];
        m->ring[0] = MBOX_NewObject(gWprObjList[6], m->mat[0], -1, 0x42000);
        ZOID_SetColor(m->ring[0], &clr);
        m->ring[1] = MBOX_NewObject(gWprObjList[6], m->mat[1], -1, 0x42000);
        ZOID_SetColor(m->ring[1], &clr);
        m->alpha[0] = 255;
        m->alpha[1] = 255;
        m->step[0] = 0;
        m->step[1] = -1;
        m->scale[0] = 1.0f;
        m->scale[1] = 1.0f;
        m->elapsed[0] = 0.0f;
        m->elapsed[1] = 0.0f;
        MBOX_HideObject(m->ring[1], 0, 15);
        t->flags |= 3;
    }
    m->timer += gFrameSeconds;
    if (m->timer >= 0.25f) {
        if (m->phase) {
            m->phase = 0;
            ZOID_SetTexture(m->base, &gTexDefPtrs[gMineTextures[4] >> 10].base[gMineTextures[4] & 1023], -1);
        } else {
            m->phase = 1;
            ZOID_SetTexture(m->base, &gTexDefPtrs[gMineTextures[gCarTeam[t->owner]] >> 10].base[gMineTextures[gCarTeam[t->owner]] & 1023], -1);
        }
        m->timer = 0.0f;
    }
    for (i = 0; i < 2; i++) {
        if (m->step[i] < 0) continue;
        if (m->step[i] >= 45) {
            m->scale[i] = 1.0f;
            m->alpha[i] = 255;
            clrPtr = ZOID_GetColor(m->ring[i]);
            clrPtr->c.a = 255;
            CopyUV(t->mat, m->mat[i]);
            m->step[i] = -1;
            MBOX_HideObject(m->ring[i], 0, 15);
            continue;
        }
        if (m->elapsed[i] >= 0.0333333f) {
            CopyUV(t->mat, m->mat[i]);
            for (j = 0; j < 3; j++) {
                for (k = 0; k < 3; k++) {
                    m->mat[i][j][k] *= m->scale[i];
                }
            }
            m->scale[i] += 0.25f;
            m->elapsed[i] = 0.0f;
            m->step[i]++;
        }
        m->elapsed[i] += gFrameSeconds;
        if (m->step[i] >= 20) {
            if (m->alpha[i] > 8) m->alpha[i] -= 8;
            clrPtr = ZOID_GetColor(m->ring[i]);
            clrPtr->c.a = m->alpha[i];
        }
        if (m->step[i] >= 25) {
            j = !i;
            if (m->step[j] < 0) {
                m->step[j] = 0;
                MBOX_ShowObject(m->ring[j], 0, 15);
            }
        }
    }
    t->age += gFrameSeconds;
    if (!(t->flags & 2) || (t->flags & 4) || t->age > 120.0f) {
        acol = gAlwaysColList.active;
        while (acol) {
            acolnext = acol->next;
            if (acol->target == t) FreeStatLLE(&gAlwaysColList, acol);
            acol = acolnext;
        }
        for (i = 0; i < 4; i++) {
            if (gPlayerMines[t->owner].target[i] == t) {
                gPlayerMines[t->owner].target[i] = 0;
                gPlayerMines[t->owner].count--;
                break;
            }
        }
        if (t->age > 120.0f) ProximityDamage(t->position, t->position, t->owner, 400.0f, 1600);
        StartBlast(t->position, 0, 0.4f, 1);
        if (m->ring[0] != -1) {
            ZOID_RemoveObject(m->ring[0]);
            m->ring[0] = -1;
        }
        if (m->ring[1] != -1) {
            ZOID_RemoveObject(m->ring[1]);
            m->ring[1] = -1;
        }
        if (m->base != -1) {
            ZOID_RemoveObject(m->base);
            m->base = -1;
        }
        m->active = 0;
        ZOID_RemoveObject(t->model);
        t->flags = 0;
        FreeStatLLE(&gTargetList, t);
        ReleaseEnviron(v, 1);
    }
}
