/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AnimateNewBlast (0x80090FEC, 546 words) = AnimateNewBlast, MAT3 visuals.c (GC 0x80057CC8, static;
 * the tVisual callback StartNewBlast installs). Direct typed port of the MAT3 body (locals i, b, clr, x, y
 * in the GC DWARF order) with the N64 differences: the glow is removed only when expansion_pak_present is set or
 * gTrackNum >= 6, the glow alpha steps by 31 and restarts at 95 (no `< 56` clamp), and the glow colour/scale
 * update also requires gNumPlayers < 4.
 * Helpers (all inlined, as in retail): Random = Random with the libc rand (game_rand);
 * ZOID_SetColor; ZOID_RemoveObject -> ZOID_RemoveObjectTree -> _RemoveObjectTree
 * (RemoveObjectTree, called with (objnum, 0, 0)): the GC zoid.c two-level wrapper chain, which also
 * supplies 12 of the frame's inline slots. ReleaseEnviron, PitchUV = func_80090F44,
 * YawUV = YawUV.
 * Levers: gFrameSeconds volatile, as in MAT3 (per-use lui/addiu + lwc1 0(reg));
 * `if (b->mbIdx == -1) continue;` in the update loop (the GC `if (b->mbIdx != -1) {...}` form gives the same
 * code with s7/s8 given to gGameCar and the 952 stride instead of the gBlastObjs base: 473 words).
 * Unused locals u0..u3 are the exact frame residual: retail frame 256 with clr at 236, x 232, y 228; without
 * them the frame is 240 and every named offset is 8 B low; u0/u1 (above clr) and u2/u3 (below y) restore all.
 * , (423/546 words).
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;

typedef union tRGBA { unsigned char c[4]; unsigned int rgba; } tRGBA;
typedef struct tUVMatrix { float uvs[3][3]; float pos[3]; } tUVMatrix;
typedef struct tBlastObj {
    int mbIdx;                  /* 0x00 */
    tUVMatrix mat;              /* 0x04 */
    float scale;                /* 0x34 */
    float scaleInc;             /* 0x38 */
    float yaw;                  /* 0x3C */
    float yawInc;               /* 0x40 */
    float pitch;                /* 0x44 */
    float pitchInc;             /* 0x48 */
    unsigned char alpha;        /* 0x4C */
    unsigned char alphaInc;     /* 0x4D */
    signed short unused;        /* 0x4E */
    float ht;                   /* 0x50 */
} tBlastObj;
typedef struct tBlastStruct {
    tBlastObj blastObjs[4];     /* 0x000 */
    int glowIdx;                /* 0x150 */
    tUVMatrix mat;              /* 0x154 */
    unsigned char alpha;        /* 0x184 */
    unsigned char alphaCnt;     /* 0x185 */
    unsigned char ralpha;       /* 0x186 */
    unsigned char ralphaCnt;    /* 0x187 */
    float lifeTime;             /* 0x188 */
    float incTime;              /* 0x18C */
} tBlastStruct;
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct tVisual { void *next; u8 pad4[2]; s16 objnum; s16 slot; u8 padA[0x14 - 0xA]; void *func; } tVisual;
typedef struct tCarData { u8 pad0[8]; float dr_pos[3]; u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad14_308[0x380 - 0x308]; void *input; u8 pad14_384[0x3B8 - 0x384]; } tCarData;
typedef struct tModelData { u8 pad0[0x640]; s8 crashflag; u8 pad641[0x6C4 - 0x641]; s16 moving_state; u8 pad6C6[0x808 - 0x6C6]; } tModelData;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjInstance { unsigned int flags, w4; void *mat; float scale; float w; unsigned short index; short child, next, mode; void *light, *tex[4], *rect, *clip, *pal; unsigned int color; unsigned int envcolor; } ObjInstance;

extern s32 gPauseState;
extern volatile f32 gFrameSeconds;
/* On the PC the blasts are pc_blasts, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6), with this view's MAT3 field names. */
#define gBlastObjs pc_blasts
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern ObjInstance ObjInstanceList[];        /* ObjInstanceList */
extern s8 expansion_pak_present;
extern s8 gTrackNum;
extern s16 gNumPlayers;
extern int rand_seed;

void RemoveObjectTree(s16 objnum, s32 a, s32 b);     /* ZOID_RemoveObject */
void ReleaseEnviron(tVisual *v, s32 unlink);
void PitchUV(float angle, float uvs[3][3]);
void YawUV(float angle, float uvs[3][3]);

int game_rand(void);

f32 Random(f32 max);

void ZOID_RemoveObject(s16 objnum);   /* the game's own ZOID_RemoveObject (this file had a static copy: the same code) */

void ZOID_SetColor(s16 objnum, tRGBA *col);

void AnimateNewBlast(tVisual *v, s16 op)
{
    int i;
    tBlastObj *b;
    int u0, u1;
    tRGBA clr;
    float x;
    float y;
    int u2, u3;
    if (gPauseState) return;
    gBlastObjs[v->slot].lifeTime -= gFrameSeconds;
    if (gBlastObjs[v->slot].lifeTime <= 0.0f || PC_MODEL_MOVING_STATE(v->slot) >= 0) {
        for (i = 0; i < 4; i++) {
            b = &gBlastObjs[v->slot].blastObjs[i];
            if (b->mbIdx != -1) ZOID_RemoveObject(b->mbIdx);
            b->mbIdx = -1;
        }
    }
    if (!PC_MODEL_CRASHFLAG(v->slot)) {
        if (expansion_pak_present || gTrackNum >= 6) {
            if (gBlastObjs[v->slot].glowIdx != -1) {
                ZOID_RemoveObject(gBlastObjs[v->slot].glowIdx);
                gBlastObjs[v->slot].glowIdx = -1;
            }
        }
        for (i = 0; i < 4; i++) {
            b = &gBlastObjs[v->slot].blastObjs[i];
            if (b->mbIdx != -1) ZOID_RemoveObject(b->mbIdx);
            b->mbIdx = -1;
        }
        goto release;
    }
    if (!op) {
release:
        ReleaseEnviron(v, 1);
        return;
    }
    gBlastObjs[v->slot].mat.pos[0] = PC_CAR_POS(v->slot)[0];
    gBlastObjs[v->slot].mat.pos[1] = PC_CAR_POS(v->slot)[1];
    gBlastObjs[v->slot].mat.pos[2] = PC_CAR_POS(v->slot)[2];
    gBlastObjs[v->slot].incTime -= gFrameSeconds;
    if (gBlastObjs[v->slot].incTime <= 0.0f) {
        if (gBlastObjs[v->slot].alphaCnt == 0) gBlastObjs[v->slot].alpha -= 31;
        else if (gBlastObjs[v->slot].alphaCnt == 1) gBlastObjs[v->slot].alpha += 31;
        else if (gBlastObjs[v->slot].alphaCnt > 1) {
            gBlastObjs[v->slot].alphaCnt = (int)Random(3.0f) * -1;
            gBlastObjs[v->slot].alpha = 95;
        }
        gBlastObjs[v->slot].alphaCnt++;
        if (gNumPlayers < 4 && (expansion_pak_present || gTrackNum >= 6)) {
            clr.c[0] = 255;
            clr.c[1] = 255;
            clr.c[2] = 255;
            clr.c[3] = gBlastObjs[v->slot].alpha;
            ZOID_SetColor(gBlastObjs[v->slot].glowIdx, &clr);
            if (ObjInstanceList[gBlastObjs[v->slot].glowIdx].scale < 1.0f)
                ObjInstanceList[gBlastObjs[v->slot].glowIdx].scale += 0.05f;
        }
        for (i = 0; i < 4; i++) {
            b = &gBlastObjs[v->slot].blastObjs[i];
            if (b->mbIdx == -1) continue;
            b->mat.uvs[0][0] = b->mat.uvs[1][1] = b->mat.uvs[2][2] = b->scale;
            b->mat.uvs[0][1] = b->mat.uvs[1][0] = b->mat.uvs[2][0] = 0.0f;
            b->mat.uvs[0][2] = b->mat.uvs[1][2] = b->mat.uvs[2][1] = 0.0f;
            switch (i) {
                case 0: x = 0.0f; y = 0.0f; break;
                case 1: x = 0.5f * b->ht; y = 0.5f * b->ht; break;
                case 2: x = -0.5f * b->ht; y = -0.5f * b->ht; break;
                case 3: x = -0.5f * b->ht; y = 0.5f * b->ht; break;
            }
            b->mat.pos[0] = x + PC_CAR_POS(v->slot)[0];
            b->mat.pos[2] = y + PC_CAR_POS(v->slot)[2];
            b->mat.pos[1] = b->ht + PC_CAR_POS(v->slot)[1] - 3.0f;
            b->ht += 0.2f;
            PitchUV(b->pitch, b->mat.uvs);
            YawUV(b->yaw, b->mat.uvs);
            b->scale += b->scaleInc;
            b->scaleInc -= 0.002f;
            b->yaw += b->yawInc;
            b->pitch += b->pitchInc;
            if (gBlastObjs[v->slot].lifeTime < 0.2f) {
                clr.c[0] = 244;
                clr.c[1] = 205;
                clr.c[2] = 20;
                clr.c[3] = b->alpha;
                if (b->alpha > b->alphaInc) b->alpha -= b->alphaInc;
                ZOID_SetColor(b->mbIdx, &clr);
            }
        }
        gBlastObjs[v->slot].incTime = 0.0333333f;
    }
}
