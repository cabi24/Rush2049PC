/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * visuals.c (MAT3 GameSource/src/visuals.c), N64 game code:
 *   entity_tick_main  0x800930A4, 671 words = AnimateRocketFlame (GC 0x80055C3C, static; slot arrives in s5)
 *   drone_ai_update   0x80093B20, 820 words = AnimateFrame       (GC 0x8005667C, static; tVisual callback)
 *   GetCarsIRColor    (GC 0x800563B0) and ZOID_SetObjectDef are inlined into AnimateFrame; they are written
 * non-static and kept off the keep list so their deleted-procedure stubs carry a symbol.
 * Built on (MAT3 Rocket/Frame
 * bodies, N64 field offsets, gTrackBaseColors as s32, 4 palette samples at 0x344) and the first pass.
 * MATCH in group visual_tire_module and in the whole-program unit.
 *
 * N64 differences from the GC body: no per-camera Show/Hide pass in AnimateRocketFlame; no trim (tobj) or
 * body-type-3 extra part (eobj), no rear-view test and no IR colour smoothing (weights) in AnimateFrame.
 * Shaping levers (each measured):
 *  - GetCarsIRColor: the palette table expression gTrackColors[gTrackNum] is used directly (one CSE web, a3),
 *    four index variables declared after a per-channel sum variable (v0), blend through unsigned a/b.
 *  - PickCar is declared int as in MAT3 (`int PickCar`): gives retail's beq operand order.
 *  - `tex` names ObjInstanceList[hobj].index (retail keeps it in v1 ahead of the branch).
 * - `if (car->lightIdx) {}` (compiled out, no code) before ZOID_ObjectFlags(hobj): a read holder
 *    that lets uopt PRE load the camera ahead of the index test and reload it after ZOID_SetObjectDef's
 *    store, as retail does; the camera web then interferes with tex and takes a3 (oracle: p1:w505=c6).
 * Unused locals (exact frame residuals only; each named local takes a 4-byte slot, inlined callee
 * locals/params follow the caller's in call order):
 *  - entity_tick_main: retail frame 368; named offsets flames 344..352, clr 340, scale1 336, pos 324..332
 *    (the GC's unreferenced `float r` has no slot). Without u1..u6 the frame is 344 with every named local
 *    24 B low; u1..u6 (6 x 4 B, below b) reproduce every offset.
 *  - drone_ai_update: retail frame 400; hulk 396, slot 392 (s16; a 2-byte slot above it: u0), def 380,
 *    hobj 372, sobj 360, clr 356, sclr1 352, sclr2 348, nextclr 332; inline GetCarsIRColor clr 308,
 *    sclr2 300, rgb 296. tobj/eobj (MAT3 donor names, unused on N64) fill 368/364; rear/u5 fill 340/336
 *    beside tex 344; in GetCarsIRColor u6 (304) and u8..u11 (four slots below its locals).
 *    Without these ten fillers (u0 is 2 bytes) the frame is 368 and 82 words differ, every one a stack
 *    immediate (aligned rows normalised: 0); without u1..u6 entity_tick_main differs in 47 stack words.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

float sqrtf(float);
float fabsf(float);
#pragma intrinsic (sqrtf)
#pragma intrinsic (fabsf)

typedef union {
    u8 b[4];
    u32 w;
} Color;

/* 3x3 matrix followed by a translation; CopyUV copies m[9] */
typedef struct Node {
    /* 0x00 */ f32 m[9];
    /* 0x24 */ f32 t[3];
} Node;

/* table at 0x8012E700, 0x44 bytes each; flags bit 31 = hidden */
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ModelSlot {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u32 w4;
    /* 0x08 */ Node *node;
    /* 0x10 */ f32 scale, w;
    /* 0x18 */ u16 tex;
    /* 0x1A */ s16 child;
    /* 0x1C */ s16 sibling;
    /* 0x1E */ s16 mode;
    /* 0x20 */ void *light, *texture[4], *rect, *clip, *pal;
    /* 0x60 */ u32 colA;
    /* 0x64 */ u32 colB;
} ModelSlot;

/* table at 0x80139320, 0x40 bytes each: four model-slot ids per car */
typedef struct CarModels {
    /* 0x00 */ s32 id[4];
    /* 0x10 */ s32 wheelA;
    /* 0x14 */ s32 wheelB;
    /* 0x18 */ s32 pad18[10];
} CarModels;

/* gGameCar element (0x3B8 bytes) */
typedef struct Car {
    /* 0x000 */ u8 pad0[0xE8];
    /* 0x0E8 */ u32 flagsE8;
    /* 0x0EC */ u8 padEC[0xF8 - 0xEC];
    /* 0x0F8 */ s16 sF8;
    /* 0x0FA */ u8 padFA[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 padFA_308[0x344 - 0x308];
    /* 0x344 */ u16 pal[4];
    /* 0x34C */ u32 irColor;
    /* 0x350 */ u32 prevIRColor;
    /* 0x354 */ u8 pad354[4];
    /* 0x358 */ u8 pad358[2];
    /* 0x35A */ s8 fade;
    /* 0x35B */ u8 pad35B;
    /* 0x35C */ s8 lightIdx;
    /* 0x35D */ s8 lightMode;
    /* 0x35E */ u8 pad35E;
    /* 0x35F */ s8 nameSet;
    /* 0x360 */ u32 alpha;
    /* 0x364 */ u8 pad364[0x374 - 0x364];
    /* 0x374 */ f32 scale[3];
    /* 0x380 */ void *input;         /* the car's input record, a host pointer */
    /* 0x384 */ s8 b384;
    /* 0x385 */ u8 pad385[0x38C - 0x385];
    /* 0x38C */ u32 f38C;
    /* 0x390 */ u8 pad390[0x3A1 - 0x390];
    /* 0x3A1 */ u8 b3A1;
    /* 0x3A2 */ u8 pad3A2[0x3B8 - 0x3A2];
} Car;

/* model element (0x808 bytes) */
typedef struct CarState {
    /* 0x000 */ u8 pad0[8];
    /* 0x008 */ u8 mode;
    /* 0x009 */ u8 pad9[0x3D0 - 9];
    /* 0x3D0 */ f32 f3D0;
    /* 0x3D4 */ u8 pad3D4[0x5F4 - 0x3D4];
    /* 0x5F4 */ f32 dist;
    /* 0x5F8 */ u8 pad5F8[0x6C4 - 0x5F8];
    /* 0x6C4 */ s16 s6C4;
    /* 0x6C6 */ u8 pad6C6[0x7D0 - 0x6C6];
    /* 0x7D0 */ s16 s7D0;
    /* 0x7D2 */ u8 pad7D2[0x808 - 0x7D2];
} CarState;

/* the entity drone_ai_update is called with */
typedef struct Entity {
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
    /* 0x00 */ void *pad0;
    /* 0x04 */ u8 pad4[2];
    /* 0x06 */ s16 slot;
    /* 0x08 */ s16 car;
    /* 0x0A */ u8 padA[0x14 - 0xA];
    /* 0x14 */ intptr_t field14;
} Entity;

extern ModelSlot ObjInstanceList[];
extern CarModels gCarParts[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s32 gRace;
extern s32 rand_seed;          /* rng state */
extern Color D_8011B464;
extern s32 gGameState;
extern s8 gInvisibleCar;
extern s8 gCarMines;
extern s8 gFogIntensity;
extern volatile u8 gNumModels;

extern s8 gTrackNum;
extern s16 gNumWindows;
/* The scene's object definitions by kind: three a car slot from 1, the mine a car shows at 236 (MAT3
 * gObjList[550]). */
extern u16 gObjList[];
extern u8 car_mine_model_name[];
extern u8 *gTrackColors[];        /* palette base per track */
extern s32 gTrackBaseColors[];         /* stride 4 */
extern s32 SheenColors[][2];

void MBOX_HideObject(s32, s32, u32);
void MBOX_ShowObject(s32, s32, u32);
s32 CheckVisible(Entity *,s32,s32);
void CopyUV(f32 *, f32 *);
void ScaleUV(f32 *, f32 *, f32);
void SetFrameColor(s16, u32 *);
void SetSheenColor(s16, Color *, Color *);
int PickCar(s16, u8);
s32 MBOX_FindObject_WORK(char *, s8, s8, s8);

f32 Random(f32 max);

Node *ZOID_FindMatrix(s16 objnum);

void ZOID_SetColor(s16 objnum, Color *color);

void AnimateRocketFlame(s16 ipa_s5, s32 flag);

/* ZOID_SetObjectDef (inlined on the N64: this file had a second copy of it), ZOID_ObjectFlags, ZOID_SetObjectFlags */
void ZOID_SetObjectDef(s16 objnum, u16 def);
u32 ZOID_ObjectFlags(s16 objnum);


/* GetCarsIRColor: N64 form (no colour smoothing); inlined into AnimateFrame. */
Color GetCarsIRColor(Car *car, CarState *m)
{
    Color clr;
    int u6;
    Color sclr2;
    unsigned char rgb[3];
    float d1;
    float d2;
    unsigned int sum;
    unsigned int i0;
    unsigned int i1;
    unsigned int i2;
    unsigned int i3;
    unsigned int a;
    unsigned int b;
    int u8,u9,u10,u11;
    i0=(car->pal[0]&0xf800)>>11;
    i1=(car->pal[1]&0xf800)>>11;
    i2=(car->pal[2]&0xf800)>>11;
    i3=(car->pal[3]&0xf800)>>11;
    sum = ((Color *)gTrackColors[gTrackNum])[i0].b[0]+((Color *)gTrackColors[gTrackNum])[i1].b[0]+((Color *)gTrackColors[gTrackNum])[i2].b[0]+((Color *)gTrackColors[gTrackNum])[i3].b[0];
    rgb[0] = sum >> 2;
    sum = ((Color *)gTrackColors[gTrackNum])[i0].b[1]+((Color *)gTrackColors[gTrackNum])[i1].b[1]+((Color *)gTrackColors[gTrackNum])[i2].b[1]+((Color *)gTrackColors[gTrackNum])[i3].b[1];
    rgb[1] = sum >> 2;
    sum = ((Color *)gTrackColors[gTrackNum])[i0].b[2]+((Color *)gTrackColors[gTrackNum])[i1].b[2]+((Color *)gTrackColors[gTrackNum])[i2].b[2]+((Color *)gTrackColors[gTrackNum])[i3].b[2];
    rgb[2] = sum >> 2;
    if (m->dist >= 20.0f) {
        d1 = 0.025f * (m->dist - 20.0f);
        if (d1 > 1.0f)
            return ((Color *)gTrackColors[gTrackNum])[gTrackBaseColors[gTrackNum]];
        if (d1 > 0.0f) {
            sclr2.w = ((Color *)gTrackColors[gTrackNum])[gTrackBaseColors[gTrackNum]].w;
            d2 = 1.0f - d1;
            a = rgb[0];
            b = sclr2.b[0];
            rgb[0] = a * d2 + b * d1;
            a = rgb[1];
            b = sclr2.b[1];
            rgb[1] = a * d2 + b * d1;
            a = rgb[2];
            b = sclr2.b[2];
            rgb[2] = a * d2 + b * d1;
        }
    }
    clr.b[0] = rgb[0];
    clr.b[1] = rgb[1];
    clr.b[2] = rgb[2];
    clr.b[3] = 255;
    return clr;
}

/* AnimateFrame */
void AnimateFrame(Entity *ent, s16 alive)
{
    int hulk;
    s16 u0;
    s16 slot;
    CarState *m;
    Car *car;
    int def;
    u32 flags;
    u32 hobj;
    u32 tobj;
    u32 eobj;
    u32 sobj;
    Color clr;
    Color sclr1;
    Color sclr2;
    int tex;
    int rear;
    int u5;
    Color nextclr;

    slot = ent->car;
    m = PC_MODEL(slot);
    car = PC_CAR(slot);
    def = ObjInstanceList[ent->slot].tex;
    if (alive == 0) {
        if (ent->slot >= 0) {
            MBOX_HideObject(ent->slot, 1, 15);
        }
        ent->field14 = 0;
        ent->slot = -1;
        return;
    }
    hulk = (car->flagsE8 & 0x10) != 0;
    sobj = gCarParts[slot].wheelB;
    hobj = gCarParts[slot].wheelA;
    if (m->mode == 2) {
        AnimateRocketFlame(slot, hulk);
    }

    nextclr = GetCarsIRColor(car, m);
    clr.w = nextclr.w;
    car->prevIRColor = car->irColor;
    car->irColor = clr.w;
    sclr1.w = SheenColors[gTrackNum][0];
    sclr2.w = SheenColors[gTrackNum][1];

    if (car->fade != 0) {
        if (car->alpha < 0x20) {
            car->alpha = 0;
        } else {
            PC_EACH30(car->alpha -= 0x19;)
        }
    } else {
        if (car->alpha >= 0xE1) {
            car->alpha = 0xFF;
        } else {
            PC_EACH30(car->alpha += 0x19;)
        }
    }
    sclr1.b[3] = car->alpha;
    sclr2.b[3] = car->alpha;
    if (gRace == 2 && slot > 0) {
        clr.b[3] = 0x80;
    } else if (gRace == 6 && (car->f38C & 1)) {
        clr.b[3] = car->b3A1;
        sclr1.b[3] = car->b3A1;
        sclr2.b[3] = car->b3A1;
    } else {
        clr.b[3] = 0xFF;
    }
    SetFrameColor(slot, &clr.w);
    SetSheenColor(slot, &sclr1, &sclr2);

    if (hulk) {
        car->nameSet = 0;
        if (PickCar(slot, 13) != def) {
            ZOID_SetObjectDef(ent->slot, PickCar(slot, 13));
        }
        MBOX_HideObject(hobj, 1, 15);
        MBOX_HideObject(sobj, 1, 15);
        CheckVisible(ent,1,car->lightIdx);
        return;
    }
    if ((gGameState & 0x100) && gCarMines != 0 && car->nameSet == 0) {
        gObjList[236] = MBOX_FindObject_WORK((char *)car_mine_model_name, 0, gNumModels - 1, 0);
        car->nameSet = 1;
    }
    if ((gGameState & 0x100) && gCarMines == 0 && car->nameSet != 0) {
        car->nameSet = 0;
    }
    if (car->nameSet != 0) {
        if (def != gObjList[236]) {
            ZOID_SetObjectDef(ent->slot, gObjList[236]);
        }
        MBOX_HideObject(hobj, 1, 15);
        MBOX_HideObject(sobj, 1, 15);
        CheckVisible(ent,car->lightMode>=2,car->lightIdx);
        return;
    }
    if (gGameState & 0x100) return;
    CheckVisible(ent,car->lightMode>=2,car->lightIdx);
    if (car->fade != 0) {
        if (car->alpha < 0x11) {
            MBOX_HideObject(sobj, 1, 15);
        } else {
            sclr1.b[3] = car->alpha;
            sclr2.b[3] = car->alpha;
            SetSheenColor(slot, &sclr1, &sclr2);
        }
    } else if ((gRace == 6 || gRace == 4) && gNumWindows >= 3) {
        MBOX_HideObject(sobj, 1, 15);
    } else if ((gGameState & 0x7C0000) && gFogIntensity != 0) {
        MBOX_HideObject(sobj, 1, 15);
    } else if (car->lightIdx < 0 || car->lightMode >= 2) {
        MBOX_ShowObject(sobj, 0, 15);
    } else {
        MBOX_HideObject(sobj, 1, 1 << car->lightIdx);
    }
    if (PickCar(slot, m->mode) != def) {
        ZOID_SetObjectDef(ent->slot, PickCar(slot, m->mode));
    }
    if (gRace == 6 && m->s6C4 >= 0) {
        ObjInstanceList[ent->slot].flags |= 0x80000000;
    } else if (gInvisibleCar != 0 || (car->flagsE8 & 8)) {
        ObjInstanceList[ent->slot].flags ^= 0x80000000;
    } else {
        ObjInstanceList[ent->slot].flags &= 0x7FFFFFFF;
    }
    if (car->lightMode != 1 || gNumWindows >= 2) {
        MBOX_HideObject(hobj, 1, 15);
    } else {
        if (car->lightIdx) {}
        flags = ZOID_ObjectFlags(hobj);
        tex = ObjInstanceList[hobj].tex;
        if (tex != (int)gObjList[(s16)(slot * 3 + 1)]) {
            ZOID_SetObjectDef(hobj, gObjList[(s16)(slot * 3 + 1)]);
        }
        if (flags & (0x100 << car->lightIdx)) {
            MBOX_ShowObject(hobj, 0, 1 << car->lightIdx);
        }
        if (gInvisibleCar != 0 || (car->flagsE8 & 8)) {
            ObjInstanceList[hobj].flags ^= 0x80000000;
        } else {
            ObjInstanceList[hobj].flags &= 0x7FFFFFFF;
        }
    }
}

/* AnimateRocketFlame */
void AnimateRocketFlame(s16 slot, int hulk)
{
{
    CarState *m;
    Car *car;
    Node *mat;
    Node *flames[3];
    Color clr = { {255,255,255,255} };
    float scale1;
    float pos[3];
    int mph;
    int b;
    int u1, u2, u3, u4, u5, u6;
    m=PC_MODEL(slot);
    car=PC_CAR(slot);
    if(hulk) {
        MBOX_HideObject(gCarParts[slot].id[1],1,15);
        MBOX_HideObject(gCarParts[slot].id[2],1,15);
        MBOX_HideObject(gCarParts[slot].id[3],1,15);
        return;
    }
    MBOX_ShowObject(gCarParts[slot].id[1],0,15);
    MBOX_ShowObject(gCarParts[slot].id[2],0,15);
    MBOX_ShowObject(gCarParts[slot].id[3],0,15);
    if(gRace==2 && slot>0) clr.b[3]=128;
    else if(gRace==6 && (car->f38C&1))
        clr.b[3]=car->b3A1;
    ZOID_SetColor(gCarParts[slot].id[1], &clr);
    ZOID_SetColor(gCarParts[slot].id[2], &clr);
    ZOID_SetColor(gCarParts[slot].id[3], &clr);
    mat=ZOID_FindMatrix(gCarParts[slot].id[0]);
    pos[0]=mat->t[0]+2.0f*mat->m[3];
    pos[1]=mat->t[1]+2.0f*mat->m[4];
    pos[2]=mat->t[2]+2.0f*mat->m[5];
    pos[0]=pos[0]+mat->m[6]*-5.6f;
    pos[1]=pos[1]+mat->m[7]*-5.6f;
    pos[2]=pos[2]+mat->m[8]*-5.6f;
    flames[0]=ZOID_FindMatrix(gCarParts[slot].id[1]);
    CopyUV(mat->m,flames[0]->m);
    flames[0]->t[0]=pos[0];
    flames[0]->t[1]=pos[1];
    flames[0]->t[2]=pos[2];
    flames[1]=ZOID_FindMatrix(gCarParts[slot].id[2]);
    CopyUV(mat->m,flames[1]->m);
    flames[1]->t[0]=pos[0];
    flames[1]->t[1]=pos[1];
    flames[1]->t[2]=pos[2];
    flames[2]=ZOID_FindMatrix(gCarParts[slot].id[3]);
    CopyUV(mat->m,flames[2]->m);
    flames[2]->t[0]=pos[0];
    flames[2]->t[1]=pos[1];
    flames[2]->t[2]=pos[2];
    b=0;
    if(gRace==6 && car->b384==3) {
        b=1;
        car->scale[0]=0.1f;
        car->scale[1]=0.1f;
        car->scale[2]=0.2f;
    } else if((car->sF8>>2)<0) {
        car->scale[0]=0.25f-Random(0.15f);
        car->scale[1]=0.25f-Random(0.15f);
        car->scale[2]=0.55f-Random(0.1);
    } else if(m->f3D0<0.1f) {
        if(car->scale[0]<0.28f)
            car->scale[0]=0.25f-Random(0.15f);
        else {
            car->scale[0]-=PC_FSCALE(Random(0.025f)+0.01f);
            if(car->scale[0]<0.2f) car->scale[0]=0.2f;
        }
        if(car->scale[1]<0.28f)
            car->scale[1]=0.25f-Random(0.15f);
        else {
            car->scale[1]-=PC_FSCALE(Random(0.025f)+0.01f);
            if(car->scale[1]<0.2f) car->scale[1]=0.2f;
        }
        if(car->scale[2]<0.58f)
            car->scale[2]=0.55f-Random(0.1);
        else {
            car->scale[2]-=PC_FSCALE(Random(0.025f)+0.01f);
            if(car->scale[2]<0.5f) car->scale[2]=0.5f;
        }
    } else {
        mph=m->s7D0;
        scale1=0.2f+(1.35f*mph)/10000.0f;
        if(scale1>1.0f) scale1=1.0f;
        car->scale[0]=scale1-Random(0.15f);
        car->scale[1]=scale1-Random(0.15f);
        scale1=0.5f+mph/10000.0f;
        if(scale1>1.0f) scale1=1.0f;
        car->scale[2]=scale1-Random(0.1);
    }
    flames[0]->m[6]*=car->scale[0];
    flames[0]->m[7]*=car->scale[0];
    flames[0]->m[8]*=car->scale[0];
    flames[1]->m[6]*=car->scale[1];
    flames[1]->m[7]*=car->scale[1];
    flames[1]->m[8]*=car->scale[1];
    if(b)
        ScaleUV(flames[2]->m,flames[2]->m,
                car->scale[2]);
    else {
        flames[2]->m[0]*=car->scale[2];
        flames[2]->m[1]*=car->scale[2];
        flames[2]->m[2]*=car->scale[2];
        flames[2]->m[3]*=car->scale[2];
        flames[2]->m[4]*=car->scale[2];
        flames[2]->m[5]*=car->scale[2];
    }
    if(gRace==6 && m->s6C4>=0) {
        ObjInstanceList[gCarParts[slot].id[1]].flags |=0x80000000;
        ObjInstanceList[gCarParts[slot].id[2]].flags |=0x80000000;
        ObjInstanceList[gCarParts[slot].id[3]].flags |=0x80000000;
    } else if(gInvisibleCar || (car->flagsE8&8)) {
        ObjInstanceList[gCarParts[slot].id[1]].flags ^=0x80000000;
        ObjInstanceList[gCarParts[slot].id[2]].flags ^=0x80000000;
        ObjInstanceList[gCarParts[slot].id[3]].flags ^=0x80000000;
    } else {
        ObjInstanceList[gCarParts[slot].id[1]].flags &=0x7fffffff;
        ObjInstanceList[gCarParts[slot].id[2]].flags &=0x7fffffff;
        ObjInstanceList[gCarParts[slot].id[3]].flags &=0x7fffffff;
    }
}
}
