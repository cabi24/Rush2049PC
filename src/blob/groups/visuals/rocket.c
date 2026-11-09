/* rocket.c (visuals): the car's frame each frame, its IR colour and the rocket car's flames (MAT3 visuals.c,
 * AnimateFrame(), GetCarsIRColor(), AnimateRocketFlame()). */
#include "game/game.h"

float sqrtf(float);
float fabsf(float);
#pragma intrinsic (sqrtf)
#pragma intrinsic (fabsf)

typedef union Color {
    u8 b[4];
    u32 w;
} Color;

/* A scene node's matrix: its rotation (CopyUV copies the 3x3) and its position relative to its parent. */
typedef struct tUVMatrix {
    /* 0x00 */ f32 uvs[3][3];
    /* 0x24 */ f32 pos[3];
} tUVMatrix;

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields these functions use. */
typedef struct tCarData {
    /* 0x000 */ u8 pad0[0xE8];
    /* 0x0E8 */ u32 appearance;
    /* 0x0EC */ u8 padEC[0xF8 - 0xEC];
    /* 0x0F8 */ s16 rwmph;          /* the speed, mph x 4, negative backwards */
    /* 0x0FA */ u8 padFA[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 pad308[0x344 - 0x308];
    /* 0x344 */ u16 sound_flags[4]; /* each tire's road polygon's sound flags: bits 11-15 a colour of the track's */
    /* 0x34C */ u32 IRclr;          /* the colour the car's frame takes (tRGBA) */
    /* 0x350 */ u32 prevIRclr;
    /* 0x354 */ u8 pad354[4];
    /* 0x358 */ u8 pad358[2];
    /* 0x35A */ s8 in_tunnel;       /* the sheen fades out in a tunnel */
    /* 0x35B */ u8 pad35B;
    /* 0x35C */ s8 camera;          /* the window that follows this car, -1 none */
    /* 0x35D */ s8 view;            /* that window's view: 1 shows the hood, 2 and up the whole car */
    /* 0x35E */ u8 pad35E;
    /* 0x35F */ s8 carMine;         /* the car screen's "car mines" cheat shows the car as a mine */
    /* 0x360 */ u32 sheenAlpha;
    /* 0x364 */ u8 pad364[0x374 - 0x364];
    /* 0x374 */ f32 roktScale[3];   /* the rocket car's three flames' lengths */
    /* 0x380 */ void *player;       /* the car's input record, a host pointer */
    /* 0x384 */ s8 weapon_type;     /* battle.weapon.type */
    /* 0x385 */ u8 pad385[0x38C - 0x385];
    /* 0x38C */ u32 defense_active; /* battle.defense.active: bit 0 the car is fading (invisibility) */
    /* 0x390 */ u8 pad390[0x3A1 - 0x390];
    /* 0x3A1 */ u8 fadeAlpha;       /* battle.fadeAlpha */
    /* 0x3A2 */ u8 pad3A2[0x3B8 - 0x3A2];
} tCarData;

/* The model record (tModelData, docs/port/CARS.md 4), the fields these functions read. */
typedef struct tModelData {
    /* 0x000 */ u8 pad0[8];
    /* 0x008 */ u8 body_type;       /* the car, 0-12; 13 the wreck */
    /* 0x009 */ u8 pad9[0x3D0 - 9];
    /* 0x3D0 */ f32 throttle;       /* 0..1 */
    /* 0x3D4 */ u8 pad3D4[0x5F4 - 0x3D4];
    /* 0x5F4 */ f32 airdist2;       /* airdist[2]: how far the rear left tire is off the ground, feet */
    /* 0x5F8 */ u8 pad5F8[0x6C4 - 0x5F8];
    /* 0x6C4 */ s16 moving_state;   /* resurrect.moving_state: RES_IDLE, else respawning */
    /* 0x6C6 */ u8 pad6C6[0x7D0 - 0x6C6];
    /* 0x7D0 */ s16 rpm;
    /* 0x7D2 */ u8 pad7D2[0x808 - 0x7D2];
} tModelData;

/* A car visual (tVisual) on the PC: 0x28 bytes, next, data and func pointer-sized (struct pc_visual, docs/port/CARS.md
 * 3). */
typedef struct tVisual {
    /* 0x00 */ void *next;
    /* 0x08 */ u8 pad8[2];          /* index */
    /* 0x0A */ s16 objnum;
    /* 0x0C */ s16 slot;
    /* 0x0E */ u8 padE[0x20 - 0xE];  /* data, timeStamp */
    /* 0x20 */ intptr_t func;
} tVisual;
/* The colour car `car` takes this frame (its IR colour): the average of the track colours (gTrackColors) its four
 * tires' road polygons name, and, as its rear left tire gets more than 20 feet off the ground, more and more
 * (fully at 60 feet) the track's base colour (gTrackBaseColors). Alpha 255. MAT3 also smooths it with the last
 * frame's (prevIRclr, a third of the new); the N64 doesn't. */
Color GetCarsIRColor(tCarData *car, tModelData *m)
{
    Color clr;
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
    i0=(car->sound_flags[0]&0xf800)>>11;
    i1=(car->sound_flags[1]&0xf800)>>11;
    i2=(car->sound_flags[2]&0xf800)>>11;
    i3=(car->sound_flags[3]&0xf800)>>11;
    sum = ((Color *)gTrackColors[gTrackNum])[i0].b[0]+((Color *)gTrackColors[gTrackNum])[i1].b[0]+((Color *)gTrackColors[gTrackNum])[i2].b[0]+((Color *)gTrackColors[gTrackNum])[i3].b[0];
    rgb[0] = sum >> 2;
    sum = ((Color *)gTrackColors[gTrackNum])[i0].b[1]+((Color *)gTrackColors[gTrackNum])[i1].b[1]+((Color *)gTrackColors[gTrackNum])[i2].b[1]+((Color *)gTrackColors[gTrackNum])[i3].b[1];
    rgb[1] = sum >> 2;
    sum = ((Color *)gTrackColors[gTrackNum])[i0].b[2]+((Color *)gTrackColors[gTrackNum])[i1].b[2]+((Color *)gTrackColors[gTrackNum])[i2].b[2]+((Color *)gTrackColors[gTrackNum])[i3].b[2];
    rgb[2] = sum >> 2;
    if (m->airdist2 >= 20.0f) {
        d1 = 0.025f * (m->airdist2 - 20.0f);
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

/* The frame visual of car v->slot (VIS_FRAME), each frame (op 1): the IR colour and the sheen's fade (out in a
 * tunnel, 25 a frame), alpha 128 for the ghosts in a ghost race and the battle's fade; the rocket car's flames;
 * the wreck's body for a hulk (the hood and sheen hidden); the car screen's "car mines" cheat body; then, in a
 * race, the sheen (hidden when fading out, in a 3- or 4-window battle or stunt race, in fog, and in the car's own
 * window unless it shows the whole car), the body's definition, the invisible cheat and the respawn blink (the
 * battle hides a respawning car), and the hood, shown only in a one-window race's hood view (1). op 0 stops it:
 * the frame is hidden. */
void AnimateFrame(tVisual *v, s16 op)
{
    int hulk;
    s16 slot;
    tModelData *m;
    tCarData *car;
    int def;
    u32 flags;
    u32 hobj;
    u32 sobj;
    Color clr;
    Color sclr1;
    Color sclr2;
    int tex;
    Color nextclr;

    slot = v->slot;
    m = PC_MODEL(slot);
    car = PC_CAR(slot);
    def = ObjInstanceList[v->objnum].index;
    if (op == 0) {
        if (v->objnum >= 0) {
            MBOX_HideObject(v->objnum, 1, 15);
        }
        v->func = 0;
        v->objnum = -1;
        return;
    }
    hulk = (car->appearance & 0x10) != 0;
    sobj = gCarParts[slot].sheen;
    hobj = gCarParts[slot].hood;
    if (m->body_type == 2) {
        AnimateRocketFlame(slot, hulk);
    }

    nextclr = GetCarsIRColor(car, m);
    clr.w = nextclr.w;
    car->prevIRclr = car->IRclr;
    car->IRclr = clr.w;
    sclr1.w = SheenColors[gTrackNum][0].rgba;
    sclr2.w = SheenColors[gTrackNum][1].rgba;

    if (car->in_tunnel != 0) {
        if (car->sheenAlpha < 0x20) {
            car->sheenAlpha = 0;
        } else {
            PC_EACH30(car->sheenAlpha -= 0x19;)
        }
    } else {
        if (car->sheenAlpha >= 0xE1) {
            car->sheenAlpha = 0xFF;
        } else {
            PC_EACH30(car->sheenAlpha += 0x19;)
        }
    }
    sclr1.b[3] = car->sheenAlpha;
    sclr2.b[3] = car->sheenAlpha;
    if (gRace == kGhost && slot > 0) {
        clr.b[3] = 0x80;
    } else if (gRace == kBattle && (car->defense_active & 1)) {
        clr.b[3] = car->fadeAlpha;
        sclr1.b[3] = car->fadeAlpha;
        sclr2.b[3] = car->fadeAlpha;
    } else {
        clr.b[3] = 0xFF;
    }
    SetFrameColor(slot, &clr.w);
    SetSheenColor(slot, &sclr1, &sclr2);

    if (hulk) {
        car->carMine = 0;
        if (PickCar(slot, 13) != def) {
            ZOID_SetObjectDef(v->objnum, PickCar(slot, 13));
        }
        MBOX_HideObject(hobj, 1, 15);
        MBOX_HideObject(sobj, 1, 15);
        CheckVisible(v,1,car->camera);
        return;
    }
    if ((gGameState & GS_CAR) && gCarMines != 0 && car->carMine == 0) {
        gObjList[OBJLIST_MINE] = MBOX_FindObject_WORK((char *)car_mine_model_name, 0, gNumModels[0] - 1, 0);
        car->carMine = 1;
    }
    if ((gGameState & GS_CAR) && gCarMines == 0 && car->carMine != 0) {
        car->carMine = 0;
    }
    if (car->carMine != 0) {
        if (def != gObjList[OBJLIST_MINE]) {
            ZOID_SetObjectDef(v->objnum, gObjList[OBJLIST_MINE]);
        }
        MBOX_HideObject(hobj, 1, 15);
        MBOX_HideObject(sobj, 1, 15);
        CheckVisible(v,car->view>=2,car->camera);
        return;
    }
    if (gGameState & GS_CAR) return;
    CheckVisible(v,car->view>=2,car->camera);
    if (car->in_tunnel != 0) {
        if (car->sheenAlpha < 0x11) {
            MBOX_HideObject(sobj, 1, 15);
        } else {
            sclr1.b[3] = car->sheenAlpha;
            sclr2.b[3] = car->sheenAlpha;
            SetSheenColor(slot, &sclr1, &sclr2);
        }
    } else if ((gRace == kBattle || gRace == kStunt) && gNumWindows >= 3) {
        MBOX_HideObject(sobj, 1, 15);
    } else if ((gGameState & GS_RACE_MASK) && gFogIntensity != 0) {
        MBOX_HideObject(sobj, 1, 15);
    } else if (car->camera < 0 || car->view >= 2) {
        MBOX_ShowObject(sobj, 0, 15);
    } else {
        MBOX_HideObject(sobj, 1, 1 << car->camera);
    }
    if (PickCar(slot, m->body_type) != def) {
        ZOID_SetObjectDef(v->objnum, PickCar(slot, m->body_type));
    }
    if (gRace == kBattle && m->moving_state >= 0) {
        ObjInstanceList[v->objnum].flags |= 0x80000000;
    } else if (gInvisibleCar != 0 || (car->appearance & 8)) {
        ObjInstanceList[v->objnum].flags ^= 0x80000000;
    } else {
        ObjInstanceList[v->objnum].flags &= 0x7FFFFFFF;
    }
    if (car->view != 1 || gNumWindows >= 2) {
        MBOX_HideObject(hobj, 1, 15);
    } else {
        if (car->camera) {}
        flags = ZOID_ObjectFlags(hobj);
        tex = ObjInstanceList[hobj].index;
        if (tex != (int)gObjList[(s16)(slot * 3 + 1)]) {
            ZOID_SetObjectDef(hobj, gObjList[(s16)(slot * 3 + 1)]);
        }
        if (flags & (0x100 << car->camera)) {
            MBOX_ShowObject(hobj, 0, 1 << car->camera);
        }
        if (gInvisibleCar != 0 || (car->appearance & 8)) {
            ObjInstanceList[hobj].flags ^= 0x80000000;
        } else {
            ObjInstanceList[hobj].flags &= 0x7FFFFFFF;
        }
    }
}

/* The rocket car's three flames (body 2; AnimateFrame calls it), car `slot`: hidden for a hulk, else in the car's
 * colour (alpha as AnimateFrame's), placed behind the body (2 up, 5.6 back along the frame's axes) with its
 * rotation, and scaled along the car: small with weapon 3 in a battle, flickering short backwards or off the
 * throttle (shrinking back to it), else growing with the rpm; hidden while respawning in a battle, blinking with
 * the invisible cheat or the respawn. MAT3 also hides them in the car's own window unless it shows the whole car. */
void AnimateRocketFlame(s16 slot, int hulk)
{
{
    tModelData *m;
    tCarData *car;
    tUVMatrix *mat;
    tUVMatrix *flames[3];
    Color clr = { {255,255,255,255} };
    float scale1;
    float pos[3];
    int mph;
    int b;
    m=PC_MODEL(slot);
    car=PC_CAR(slot);
    if(hulk) {
        MBOX_HideObject(gCarParts[slot].extraparts[0],1,15);
        MBOX_HideObject(gCarParts[slot].extraparts[1],1,15);
        MBOX_HideObject(gCarParts[slot].extraparts[2],1,15);
        return;
    }
    MBOX_ShowObject(gCarParts[slot].extraparts[0],0,15);
    MBOX_ShowObject(gCarParts[slot].extraparts[1],0,15);
    MBOX_ShowObject(gCarParts[slot].extraparts[2],0,15);
    if(gRace==kGhost && slot>0) clr.b[3]=128;
    else if(gRace==kBattle && (car->defense_active&1))
        clr.b[3]=car->fadeAlpha;
    ZOID_SetColor(gCarParts[slot].extraparts[0], &clr);
    ZOID_SetColor(gCarParts[slot].extraparts[1], &clr);
    ZOID_SetColor(gCarParts[slot].extraparts[2], &clr);
    mat=ZOID_FindMatrix(gCarParts[slot].frame);
    pos[0]=mat->pos[0]+2.0f*mat->uvs[1][0];
    pos[1]=mat->pos[1]+2.0f*mat->uvs[1][1];
    pos[2]=mat->pos[2]+2.0f*mat->uvs[1][2];
    pos[0]=pos[0]+mat->uvs[2][0]*-5.6f;
    pos[1]=pos[1]+mat->uvs[2][1]*-5.6f;
    pos[2]=pos[2]+mat->uvs[2][2]*-5.6f;
    flames[0]=ZOID_FindMatrix(gCarParts[slot].extraparts[0]);
    CopyUV(mat->uvs,flames[0]->uvs);
    flames[0]->pos[0]=pos[0];
    flames[0]->pos[1]=pos[1];
    flames[0]->pos[2]=pos[2];
    flames[1]=ZOID_FindMatrix(gCarParts[slot].extraparts[1]);
    CopyUV(mat->uvs,flames[1]->uvs);
    flames[1]->pos[0]=pos[0];
    flames[1]->pos[1]=pos[1];
    flames[1]->pos[2]=pos[2];
    flames[2]=ZOID_FindMatrix(gCarParts[slot].extraparts[2]);
    CopyUV(mat->uvs,flames[2]->uvs);
    flames[2]->pos[0]=pos[0];
    flames[2]->pos[1]=pos[1];
    flames[2]->pos[2]=pos[2];
    b=0;
    if(gRace==kBattle && car->weapon_type==3) {
        b=1;
        car->roktScale[0]=0.1f;
        car->roktScale[1]=0.1f;
        car->roktScale[2]=0.2f;
    } else if((car->rwmph>>2)<0) {
        car->roktScale[0]=0.25f-Random(0.15f);
        car->roktScale[1]=0.25f-Random(0.15f);
        car->roktScale[2]=0.55f-Random(0.1);
    } else if(m->throttle<0.1f) {
        if(car->roktScale[0]<0.28f)
            car->roktScale[0]=0.25f-Random(0.15f);
        else {
            car->roktScale[0]-=PC_FSCALE(Random(0.025f)+0.01f);
            if(car->roktScale[0]<0.2f) car->roktScale[0]=0.2f;
        }
        if(car->roktScale[1]<0.28f)
            car->roktScale[1]=0.25f-Random(0.15f);
        else {
            car->roktScale[1]-=PC_FSCALE(Random(0.025f)+0.01f);
            if(car->roktScale[1]<0.2f) car->roktScale[1]=0.2f;
        }
        if(car->roktScale[2]<0.58f)
            car->roktScale[2]=0.55f-Random(0.1);
        else {
            car->roktScale[2]-=PC_FSCALE(Random(0.025f)+0.01f);
            if(car->roktScale[2]<0.5f) car->roktScale[2]=0.5f;
        }
    } else {
        mph=m->rpm;
        scale1=0.2f+(1.35f*mph)/10000.0f;
        if(scale1>1.0f) scale1=1.0f;
        car->roktScale[0]=scale1-Random(0.15f);
        car->roktScale[1]=scale1-Random(0.15f);
        scale1=0.5f+mph/10000.0f;
        if(scale1>1.0f) scale1=1.0f;
        car->roktScale[2]=scale1-Random(0.1);
    }
    flames[0]->uvs[2][0]*=car->roktScale[0];
    flames[0]->uvs[2][1]*=car->roktScale[0];
    flames[0]->uvs[2][2]*=car->roktScale[0];
    flames[1]->uvs[2][0]*=car->roktScale[1];
    flames[1]->uvs[2][1]*=car->roktScale[1];
    flames[1]->uvs[2][2]*=car->roktScale[1];
    if(b)
        ScaleUV(flames[2]->uvs,flames[2]->uvs,
                car->roktScale[2]);
    else {
        flames[2]->uvs[0][0]*=car->roktScale[2];
        flames[2]->uvs[0][1]*=car->roktScale[2];
        flames[2]->uvs[0][2]*=car->roktScale[2];
        flames[2]->uvs[1][0]*=car->roktScale[2];
        flames[2]->uvs[1][1]*=car->roktScale[2];
        flames[2]->uvs[1][2]*=car->roktScale[2];
    }
    if(gRace==kBattle && m->moving_state>=0) {
        ObjInstanceList[gCarParts[slot].extraparts[0]].flags |=0x80000000;
        ObjInstanceList[gCarParts[slot].extraparts[1]].flags |=0x80000000;
        ObjInstanceList[gCarParts[slot].extraparts[2]].flags |=0x80000000;
    } else if(gInvisibleCar || (car->appearance&8)) {
        ObjInstanceList[gCarParts[slot].extraparts[0]].flags ^=0x80000000;
        ObjInstanceList[gCarParts[slot].extraparts[1]].flags ^=0x80000000;
        ObjInstanceList[gCarParts[slot].extraparts[2]].flags ^=0x80000000;
    } else {
        ObjInstanceList[gCarParts[slot].extraparts[0]].flags &=0x7fffffff;
        ObjInstanceList[gCarParts[slot].extraparts[1]].flags &=0x7fffffff;
        ObjInstanceList[gCarParts[slot].extraparts[2]].flags &=0x7fffffff;
    }
}
}
