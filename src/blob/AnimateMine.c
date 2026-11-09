/* AnimateMine: a battle mine on the road (MAT3 targets.c _AnimateMine). It is the mine's environment visual,
 * called each frame: the first time it registers the mine with its owner (at most three each: a fourth retires the
 * oldest), lays it flat on the road and makes its models (the base and two glow rings in its team's colour); then
 * it flashes the base and pulses the rings, and when a car has hit it (StartMine) or it has lain for 120 seconds,
 * it blows up and frees itself. */
#include "game/game.h"

typedef union Color {struct {u8 r,g,b,a;} c;u32 word;} Color;

/* A mine's own record (MAT3 tTargetStructMine, 0x88 bytes), the target's per-kind data. */
typedef struct MineData {
    s32 dlIdx;              /* 0x00 the mine's base, a scene object */
    s32 glowdls[2];         /* 0x04 the two glow rings, scene objects (-1 none) */
    f32 flashTime;          /* 0x0C the time since the base last flashed */
    f32 mats[2][4][3];      /* 0x10 each ring's matrix: orientation rows, then the position */
    f32 glowTimes[2];       /* 0x70 each ring's time since its last step */
    s8 glowCnts[2];         /* 0x78 each ring's step of its pulse, -1 idle */
    u8 alphs[2];            /* 0x7A each ring's alpha */
    f32 sc[2];              /* 0x7C each ring's scale */
    s16 on;                 /* 0x84 the base shows the team's colour */
    s16 used;               /* 0x86 the record is in use */
} MineData;

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). */
enum n64p_Target { n64p_Target_ };
enum n64p_MineData { n64p_MineData_ };
typedef struct Target {
    enum n64p_Target next;  /* 0x00 ll: the next in its list */
    u8 flags;               /* 0x04 Flags: TARGET_SET_UP, TARGET_ACTIVE, TARGET_HIT */
    u8 pad05[7];            /* 0x05 mbIdx at 0x08 */
    s32 dlIdx;              /* 0x0C its scene object */
    s16 infoIdx;            /* 0x10 its kind: the index of its targetInfo record */
    u8 pad12[2];
    f32 mat[4][3];          /* 0x14 its matrix: orientation rows, then the position */
    f32 pos[3];             /* 0x44 its position */
    s16 type;               /* 0x50 its object */
    u8 pad52[10];           /* 0x52 radius, otype, frames */
    s8 slot;                /* 0x5C the car that hit it */
    u8 pad5D[3];
    s32 sound;              /* 0x60 */
    s8 sndState, owner;     /* 0x64, 0x65: the car that dropped it */
    u8 pad66[2];
    f32 timeStamp;          /* 0x68 the seconds it has lain on the road */
    enum n64p_MineData data; /* 0x6C its MineData (a window address) */
} Target;

/* The environment visual (MAT3 tVisual) in its PC layout: next and func are host pointers, data the target's
 * window address. */
typedef struct Visual {struct Visual *next;s16 index,model,owner;u8 pad10[2];enum n64p_Target data;f32 time;void (*callback)(struct Visual*,s16);} Visual;
#define N64P(x) _Generic((x), enum n64p_Target: (Target *)PC_N64P(x), \
    enum n64p_MineData: (MineData *)PC_N64P(x), default: (x))

/* A texture definition (MAT3 tTexture, 36 bytes): only its size matters here. */
typedef struct tTexture {u8 bytes[36];} Texture36;

/* An always-colliding entry (MAT3 tAlwaysCol): the link, then the target it stands for. */
typedef struct Link {struct Link *next;Target *target;} Link;




/* One frame of the mine (op 1), or release its visual (op 0); nothing happens while the game is paused.
 * - First frame: if the owner already has three mines, the oldest is aged past 120 s so it goes next; the mine
 *   takes a free slot of its owner's (gPlayerMines), copies the road's orientation under it, and makes its base
 *   (gWprObjList[5], white) and two rings (gWprObjList[6], gMineRingColors of the owner's team), the second hidden.
 * - Every 0.25 s the base swaps between the neutral texture (gMineTextures[4]) and the team's.
 * - Each ring grows by a quarter every 1/30 s for 45 steps, fades from step 20 and starts the other ring at step 25.
 * - It blows up (StartBlast; after 120 s also ProximityDamage, 1600 within 400 ft) once it is no longer active,
 *   has been hit or is over 120 s old: it leaves the always-colliding list and its owner's slots, its models go, and the
 *   target and the visual are freed. */
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
    if (!(t->flags & TARGET_SET_UP)) {
        if (gPlayerMines[t->owner].count == 3) {
            oldT = 0.0f;
            j = -1;
            for (i = 0; i < 4; i++) {
                if (gPlayerMines[t->owner].target[i] && gPlayerMines[t->owner].target[i]->timeStamp > oldT) {
                    j = i;
                    oldT = gPlayerMines[t->owner].target[i]->timeStamp;
                }
            }
            if (j != -1) gPlayerMines[t->owner].target[j]->timeStamp = 121.0f;
        }
        for (i = 0; i < 4; i++) {
            if (!gPlayerMines[t->owner].target[i]) {
                gPlayerMines[t->owner].target[i] = t;
                gPlayerMines[t->owner].count++;
                break;
            }
        }
        if (RoadPosition(t->pos, opos, uvs)) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) t->mat[i][j] = uvs[i][j];
            }
        }
        j = -1;
        m->dlIdx = MBOX_NewObject(gWprObjList[5], 0, t->dlIdx, 0x2080);
        clr.c.r = 255;
        clr.c.g = 255;
        clr.c.b = 255;
        clr.c.a = 255;
        ZOID_SetColor(m->dlIdx, &clr);
        ZOID_SetObjectFlags(t->dlIdx, ZOID_ObjectFlags(t->dlIdx) | 0x2000);
        ZOID_SetColor(t->dlIdx, &clr);
        ZOID_SetTexture(m->dlIdx, &gTexDefPtrs[gMineTextures[4] >> 10].texDefs[gMineTextures[4] & 1023], -1);
        m->flashTime = 0.0f;
        m->on = 0;
        clrPtr = &clr;
        clrPtr->c.r = gMineRingColors[gCarTeam[t->owner]][0];
        clrPtr->c.g = gMineRingColors[gCarTeam[t->owner]][1];
        clrPtr->c.b = gMineRingColors[gCarTeam[t->owner]][2];
        CopyUV(t->mat, m->mats[0]);
        CopyUV(t->mat, m->mats[1]);
        m->mats[0][3][0] = t->pos[0];
        m->mats[0][3][1] = t->pos[1];
        m->mats[0][3][2] = t->pos[2];
        m->mats[0][3][0] = m->mats[0][3][0] + m->mats[0][1][0] * 0.75f;
        m->mats[0][3][1] = m->mats[0][3][1] + m->mats[0][1][1] * 0.75f;
        m->mats[0][3][2] = m->mats[0][3][2] + m->mats[0][1][2] * 0.75f;
        m->mats[1][3][0] = t->pos[0];
        m->mats[1][3][1] = t->pos[1];
        m->mats[1][3][2] = t->pos[2];
        m->mats[1][3][0] = m->mats[1][3][0] + m->mats[1][1][0];
        m->mats[1][3][1] = m->mats[1][3][1] + m->mats[1][1][1]; m->mats[1][3][2] = m->mats[1][3][2] + m->mats[1][1][2];
        m->glowdls[0] = MBOX_NewObject(gWprObjList[6], m->mats[0], -1, 0x42000);
        ZOID_SetColor(m->glowdls[0], &clr);
        m->glowdls[1] = MBOX_NewObject(gWprObjList[6], m->mats[1], -1, 0x42000);
        ZOID_SetColor(m->glowdls[1], &clr);
        m->alphs[0] = 255;
        m->alphs[1] = 255;
        m->glowCnts[0] = 0;
        m->glowCnts[1] = -1;
        m->sc[0] = 1.0f;
        m->sc[1] = 1.0f;
        m->glowTimes[0] = 0.0f;
        m->glowTimes[1] = 0.0f;
        MBOX_HideObject(m->glowdls[1], 0, 15);
        t->flags |= TARGET_SET_UP | TARGET_ACTIVE;
    }
    m->flashTime += gFrameSeconds;
    if (m->flashTime >= 0.25f) {
        if (m->on) {
            m->on = 0;
            ZOID_SetTexture(m->dlIdx, &gTexDefPtrs[gMineTextures[4] >> 10].texDefs[gMineTextures[4] & 1023], -1);
        } else {
            m->on = 1;
            ZOID_SetTexture(m->dlIdx, &gTexDefPtrs[gMineTextures[gCarTeam[t->owner]] >> 10].texDefs[gMineTextures[gCarTeam[t->owner]] & 1023], -1);
        }
        m->flashTime = 0.0f;
    }
    for (i = 0; i < 2; i++) {
        if (m->glowCnts[i] < 0) continue;
        if (m->glowCnts[i] >= 45) {
            m->sc[i] = 1.0f;
            m->alphs[i] = 255;
            clrPtr = ZOID_GetColor(m->glowdls[i]);
            clrPtr->c.a = 255;
            CopyUV(t->mat, m->mats[i]);
            m->glowCnts[i] = -1;
            MBOX_HideObject(m->glowdls[i], 0, 15);
            continue;
        }
        if (m->glowTimes[i] >= 0.0333333f) {
            CopyUV(t->mat, m->mats[i]);
            for (j = 0; j < 3; j++) {
                for (k = 0; k < 3; k++) {
                    m->mats[i][j][k] *= m->sc[i];
                }
            }
            m->sc[i] += 0.25f;
            m->glowTimes[i] = 0.0f;
            m->glowCnts[i]++;
        }
        m->glowTimes[i] += gFrameSeconds;
        if (m->glowCnts[i] >= 20) {
            if (m->alphs[i] > 8) m->alphs[i] -= 8;
            clrPtr = ZOID_GetColor(m->glowdls[i]);
            clrPtr->c.a = m->alphs[i];
        }
        if (m->glowCnts[i] >= 25) {
            j = !i;
            if (m->glowCnts[j] < 0) {
                m->glowCnts[j] = 0;
                MBOX_ShowObject(m->glowdls[j], 0, 15);
            }
        }
    }
    t->timeStamp += gFrameSeconds;
    if (!(t->flags & TARGET_ACTIVE) || (t->flags & TARGET_HIT) || t->timeStamp > 120.0f) {
        acol = gAlwaysColList.head;
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
        if (t->timeStamp > 120.0f) ProximityDamage(t->pos, t->pos, t->owner, 400.0f, 1600);
        StartBlast(t->pos, 0, 0.4f, 1);
        if (m->glowdls[0] != -1) {
            ZOID_RemoveObject(m->glowdls[0]);
            m->glowdls[0] = -1;
        }
        if (m->glowdls[1] != -1) {
            ZOID_RemoveObject(m->glowdls[1]);
            m->glowdls[1] = -1;
        }
        if (m->dlIdx != -1) {
            ZOID_RemoveObject(m->dlIdx);
            m->dlIdx = -1;
        }
        m->used = 0;
        ZOID_RemoveObject(t->dlIdx);
        t->flags = 0;
        FreeStatLLE(&gTargetList, t);
        ReleaseEnviron(v, 1);
    }
}
