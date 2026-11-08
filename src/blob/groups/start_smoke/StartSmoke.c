/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * StartSmoke (0x8008EA10): spawn N smoke/dust/exhaust effect objects for car `slot`, effect `type` 0..7.
 * Behavioural ancestor: arcade game/visuals.c StartSmoke (surface/mph gates, smoke object pick, per-tire
 * position); the N64 routine is heavily expanded (8 types, state-bit toggles in gCarSmokeDir, trail
 * interpolation for types 6/7, per-type colours).
 * STATE: EQUAL in the whole-program unit. It inlines the locked Random 19x and two file-static
 * helpers. Scorer (Pi): the whole-program unit --tag score StartSmoke --with <this file> --neighbours ->
 *   "EQUAL StartSmoke: 1386 words", "locked bodies that differ in this unit: 0".
 * lever (7 words -> 0): two inlined static helpers, defined after the function with prototypes first.
 *   - get_car(slot) returns &gGameCar[slot]. car is then assigned from the inlined return value (v0), so uopt
 *     keeps it as a variable (s1) instead of substituting the address expression. That removes car's spill-temp
 *     home, and the helper's 8-byte inline area makes the named area 632. Result: retail's homes (model address
 *     92, switch type copy 84). Defined after the function: the inlined addu carries the helper's line number,
 *     and as1 breaks the aftercycles tie with `sw zero,644(sp)` (i = 0) by the lower line, as in retail.
 *   - get_mph(car) returns car->mph >> 2. Without it, uopt propagates the v0 return into `car->mph`, v0 is in
 *     use in mph's block, and mph gets v1. With it, mph is the second helper's v0 return, as in retail.
 *   Neither helper's identity is known. Neither is a kept function in retail (no tiny function computes
 *   &gGameCar[x] or loads 248(a0)). Like attach() in the locked StartWater group, they are file statics
 *   with one call site each. In the shadow unit they leave two unsymbolled `jr ra; nop` stubs before
 *   StartSmoke, which retail does not have (same as attach()'s stub before StartWater).
 * Shaping devices / hypotheses (all disclosed):
 *   - `if (type) {}` at the top of the loop body: a compiled-out read (weight-10 use). The two helper return
 *     labels add one uopt block each to type's live range (nocs 46 -> 47, save 192/47 = 4.085). type then loses
 *     callee-saved s4 to the constants 0 and 68 (save 150/36 = 4.1667), and s4/s5/s6 are permuted. The read
 * raises type's total to 202. No natural source form found.
 *   - `*(s16 *)&surface == 8`: surface is address-taken through this one use, so it stays memory-only as in
 * retail (`volatile` is wrong). Still required: plain `surface == 8` gives 1114 words.
 *   - Unused locals pB0, pB1, pB2, pC1..pC3, pD00..pD43 (50 slots: 49 s32 and one u16) are the exact frame
 *     residual. Retail frame 728; frame without them 528. With one pD fewer the frame is 720 (159 words).
 *     One more still gives 728 (8-byte granularity). They reproduce every retail named-local offset: pos 708,
 *     delta 696, spacing 692, previous 668, surface 666, mph 664, first 662, count 648, i 644, colours
 *     628..608.
 *   - Lifetime sites 2 and 4 spell 0.5 as `.5f`: uopt keys real constants by their text, so these are
 *     separate constants and are rematerialised (lui 0x3f00) as in retail. The other 0.5 uses share $f22.
 *   - Position accumulation is written `pos[k] = pos[k] + B * A`, which gives retail's add(prod,pos) and
 *     mul(A,B) operand order. `pos[1] += suspension` stays `+=` (retail add(pos,susp)).
 *   - Each colour store uses a block local: `{ s16 h = v->handle.word; ... }`. Retail colours that value v0.
 *     The identity of the helper or macro is a hypothesis (inlined ZOID_SetColor does not fit umerge's budget).
 *   - `MBOX_ShowObject(v->handle.word, first = 0, 15)` places the `first = 0` store after the a0 load.
 * - The six state-bit cases are written `if (bit) { clear; break; } set; .; break;` (lever, no device).
 */
typedef signed char s8; typedef unsigned char u8;
typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
typedef struct Car { u8 pad0[8]; f32 pos[3]; f32 dir[3]; u8 pad32[12]; f32 right[3]; f32 up[3]; f32 forward[3]; u8 pad80[36]; f32 wheel[4][3]; u8 pad164[84]; s16 mph; u8 pad250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 pad250_308[0x35C - 0x308]; s8 viewport; s8 visibility; u8 pad862[0x380 - 0x35E]; void *input; u8 pad862_384[0x3B8 - 0x384]; } Car;
typedef struct Model {u8 pad0[8];u8 type;u8 pad9[1555];u16 surface[8];u8 pad1580[304];f32 suspension[4];u8 pad1900[156];} Model;
/* PC: the handle is the s32 MBOX_NewObject returned, and id its low half: the second s16 on the N64, the first on a
 * little-endian host (docs/port/ENDIAN.md). */
typedef struct Effect { void *next; f32 basis[9]; f32 pos[3]; union { s32 word; struct {s16 id,bank;} part; } handle; s32 unknown56; f32 lifetime; f32 timestep; f32 velocity[3]; u32 flags; s16 slot; s16 object; } Effect;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Scene { u32 flags; u32 flags4; void *owner; f32 scaleA; f32 scaleB; u16 id; s16 links[3]; void *words[8]; union { u8 byte[4]; u32 word; } rgba; s32 tail; } Scene;
typedef struct Bank {u8 *data;u32 other;} Bank;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Scene ObjInstanceList[];
/* On the PC the effects' per-car arrays are PC data, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gCarSmokeDir pc_tire_fx
#define gCarBlastSmokeClr pc_smoke_shade
#define gCarWingSmokePos pc_trail_pts
extern u8 gSmokeList[];
extern u16 gFireTexs[];
extern u16 gObjList_smoke[];
extern Bank gTexDefPtrs[];
extern s8 gCarWings_players[];
extern f32 gCarMiddles[];
extern s32 smoke_white,smoke_wing_color1,smoke_wing_color2,smoke_wing_color3,smoke_road_color,smoke_dirt_color;
/* PC: the six surface colours are R,G,B,A bytes in the ROM, which the loader swaps as the words they are declared;
 * PC_RGBA gives back the word a scene node's colour (read as bytes) must hold (docs/port/ENDIAN.md). */
void ZOID_SetTexture(s16 handle,void *resource,s32 mode);
f32 NormalVector(f32 *v);
s32 ZOID_AddObject(s32 object,void *basis,s16 parent,s32 flags);
Effect *GetStatLLE(void *pool);
void StartWater(s16 slot,u32 type);
void MBOX_HideObject(s32 handle,s32 mode,u32 viewport);
void MBOX_ShowObject(s32 handle,s32 mode,u32 viewport);
f32 Random(f32 max);
f32 fabsf(f32 x);


#pragma intrinsic (fabsf)
#define C_80123960 0.0333333015f
#define C_80123964 1.00999999f
#define C_80123968 0.0700000003f
#define C_8012396C 0.0799999982f
#define C_80123990 0.200000003f
#define C_80123994 -1.14999998f
#define C_80123998 -2.5999999f
#define C_8012399C 2.5999999f
#define C_801239A0 0.100000001f
#define C_801239A4 0.075000003f
extern u16 gObjList_smoke_lite, gObjList_fire;
extern s8 gGoodFrameRate, gTrackNum, expansion_pak_present;
extern s16 gNumPlayers;

static s16 get_mph(Car *c);
static Car *get_car(s16 slot);

void StartSmoke(s16 slot, u32 type, s32 fast, f32 *offset)
{
    Effect *v;
    Car *car;
    f32 pos[3];
    f32 delta[3];
    f32 spacing;
    f32 distance;
    f32 scale;
    Model *model = PC_MODEL(slot);
    f32 previous[3];
    s16 surface = model->surface[type];
    s16 mph;
    s16 first;
    u16 pB0;
    s32 pB1;
    s32 pB2;
    s32 count;
    s32 i;
    s32 pC1, pC2, pC3;
    s32 color0 = PC_RGBA(smoke_white);
    s32 color1 = PC_RGBA(smoke_wing_color1);
    s32 color2 = PC_RGBA(smoke_wing_color2);
    s32 color3 = PC_RGBA(smoke_wing_color3);
    s32 color4 = PC_RGBA(smoke_road_color);
    s32 color5 = PC_RGBA(smoke_dirt_color);
    s32 pD00;
    s32 pD01;
    s32 pD02;
    s32 pD03;
    s32 pD04;
    s32 pD05;
    s32 pD06;
    s32 pD07;
    s32 pD08;
    s32 pD09;
    s32 pD10;
    s32 pD11;
    s32 pD12;
    s32 pD13;
    s32 pD14;
    s32 pD15;
    s32 pD16;
    s32 pD17;
    s32 pD18;
    s32 pD19;
    s32 pD20;
    s32 pD21;
    s32 pD22;
    s32 pD23;
    s32 pD24;
    s32 pD25;
    s32 pD26;
    s32 pD27;
    s32 pD28;
    s32 pD29;
    s32 pD30;
    s32 pD31;
    s32 pD32;
    s32 pD33;
    s32 pD34;
    s32 pD35;
    s32 pD36;
    s32 pD37;
    s32 pD38;
    s32 pD39;
    s32 pD40;
    s32 pD41;
    s32 pD42;
    s32 pD43;

    
    if (!gGoodFrameRate && type < 4 && !fast) return; if (pc_fps60 && !pc_frame_steps && !fast && type != 5) return;
    
    
    i = 0;
    count = 1;
    car = get_car(slot);
    mph = get_mph(car);
    mph = fabsf((f32)mph);
    
    if (*(s16 *)&surface == 8 && type < 5) return;
    if (mph < 10 && type < 4) return;
    first = 1;
    spacing = type >= 6 ? 0.75f : 2.25f;
    if (fast) {
        StartWater(slot, type);
        return;
    }
    if (type == 5) count = 5;
    else if (type < 4 || type >= 6) {
        if (gNumPlayers >= 3) return;
        if (mph >= 115) count = 4;
        else if (mph >= 100) count = 3;
        else if (mph >= 85) count = 2;
        if (type >= 6) {
            if (type == 6 && !(gCarSmokeDir[slot] & 0x4000)) count = 1;
            else if (type == 7 && !(gCarSmokeDir[slot] & 0x8000)) count = 1;
            else count += 5;
            if (mph >= 150 && count != 1) count += 3;
        }
    } else {
        if (gCarSmokeDir[slot] & 0x40) {
            gCarSmokeDir[slot] &= ~0x40;
            gCarSmokeDir[slot] |= 0x80;
            return;
        }
        if (gCarSmokeDir[slot] & 0x80) {
            gCarSmokeDir[slot] &= ~0x80;
            if (gNumPlayers >= 3) gCarSmokeDir[slot] |= 0x80000;
            return;
        }
        if (gCarSmokeDir[slot] & 0x80000) {
            gCarSmokeDir[slot] &= ~0x80000;
            return;
        }
    }
    do {
        v = GetStatLLE(gSmokeList);
        if (type) {}
        if (!v) return;
         v->timestep = C_80123960; 
        v->flags = type << 17;
        
        v->slot = slot;
        
        
        if (type == 5) {
            v->object = -1;
            v->handle.word = ZOID_AddObject(gObjList_fire, v->basis, -1, 0x52000);
            ObjInstanceList[v->handle.word].scaleB = C_80123964;
        } else {
            if (expansion_pak_present == 0 && gTrackNum >= 0 && gTrackNum < 6) {
                v->object = 0;
                v->handle.word = ZOID_AddObject(gObjList_smoke_lite, v->basis, -1, 0x52000);
            } else {
                v->object = (s32)Random(8.0f);
                v->handle.word = ZOID_AddObject(gObjList_smoke[v->object], v->basis, -1, 0x52000);
            }
        }
        if (type >= 6) v->lifetime = Random(C_80123968) + C_8012396C;
        else if (count == 1) v->lifetime = Random(1.5f) + .5f;
        else if (type == 5) v->lifetime = Random(2.25f) + 0.5f;
        else v->lifetime = Random(0.25f) + .5f;
        if (type < 5) {
            v->velocity[0] = car->dir[0];
            v->velocity[1] = car->dir[1];
            v->velocity[2] = car->dir[2];
            v->velocity[0] *= 0.125f;
            v->velocity[1] *= 0.125f;
            v->velocity[2] *= 0.125f;
            if (type == 4) {
                v->velocity[0] *= 0.5f;
                v->velocity[1] *= 0.5f;
                v->velocity[2] *= 0.5f;
            }
        } else if (type >= 6) {
            v->velocity[0] = car->dir[0];
            v->velocity[1] = car->dir[1];
            v->velocity[2] = car->dir[2];
            v->velocity[0] *= 0.5f;
            v->velocity[1] *= 0.5f;
            v->velocity[2] *= 0.5f;
        }
        switch (type) {
        case 0:
            if (gCarSmokeDir[slot] & 1) { gCarSmokeDir[slot] &= ~1; break; }
            gCarSmokeDir[slot] |= 1; v->flags |= 1;
            break;
        case 1:
            if (gCarSmokeDir[slot] & 2) { gCarSmokeDir[slot] &= ~2; break; }
            gCarSmokeDir[slot] |= 2; v->flags |= 1;
            break;
        case 2:
            if (gCarSmokeDir[slot] & 4) { gCarSmokeDir[slot] &= ~4; break; }
            gCarSmokeDir[slot] |= 4; v->flags |= 1;
            break;
        case 3:
            if (gCarSmokeDir[slot] & 8) { gCarSmokeDir[slot] &= ~8; break; }
            gCarSmokeDir[slot] |= 8; v->flags |= 1;
            break;
        case 4:
            gCarSmokeDir[slot] |= 0x40;
            if (gNumPlayers == 1) v->lifetime += 0.5f;
            if (!(gCarSmokeDir[slot] & 0x20)) {
                gCarSmokeDir[slot] |= 0x20;
                gCarBlastSmokeClr[slot] = 175;
            }
            if (gCarSmokeDir[slot] & 0x10) gCarSmokeDir[slot] &= ~0x10;
            else { gCarSmokeDir[slot] |= 0x10; v->flags |= 1; }
            pos[0] = car->pos[0];
            pos[1] = car->pos[1];
            pos[2] = car->pos[2];
            pos[0] = pos[0] + car->right[0] * (Random(5.0f) - 2.5f);
            pos[1] = pos[1] + car->right[1] * (Random(5.0f) - 2.5f);
            pos[2] = pos[2] + car->right[2] * (Random(5.0f) - 2.5f);
            pos[0] = pos[0] + car->forward[0] * (Random(7.0f) - 3.5f);
            pos[1] = pos[1] + car->forward[1] * (Random(7.0f) - 3.5f);
            pos[2] = pos[2] + car->forward[2] * (Random(7.0f) - 3.5f);
            pos[0] = pos[0] + car->up[0] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[1] = pos[1] + car->up[1] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[2] = pos[2] + car->up[2] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[1] = pos[1] + 0.75f;
            ObjInstanceList[v->handle.word].scaleA = Random(0.5f) + 0.75f;
            break;
        case 5:
            ObjInstanceList[v->handle.word].scaleA = C_80123990;
            ZOID_SetTexture(v->handle.part.id, gTexDefPtrs[gFireTexs[i] >> 10].data + (gFireTexs[i] & 0x3ff) * 36, -1);
            pos[0] = car->pos[0];
            pos[1] = car->pos[1];
            pos[2] = car->pos[2];
            v->velocity[0] = Random(4.0f) - 2.0f;
            v->velocity[2] = Random(8.0f) - 4.0f;
            pos[0] = pos[0] + car->right[0] * v->velocity[0];
            pos[1] = pos[1] + car->right[1] * v->velocity[0];
            pos[2] = pos[2] + car->right[2] * v->velocity[0];
            pos[0] = pos[0] + car->forward[0] * v->velocity[2];
            pos[1] = pos[1] + car->forward[1] * v->velocity[2];
            pos[2] = pos[2] + car->forward[2] * v->velocity[2];
            pos[0] = pos[0] + car->up[0] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[1] = pos[1] + car->up[1] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[2] = pos[2] + car->up[2] * gCarMiddles[PC_MODEL_BODY(slot)];
            break;
        case 6:
            if (gCarSmokeDir[slot] & 0x1000) { gCarSmokeDir[slot] &= ~0x1000; break; }
            gCarSmokeDir[slot] |= 0x1000; v->flags |= 1;
            break;
        case 7:
            if (gCarSmokeDir[slot] & 0x2000) { gCarSmokeDir[slot] &= ~0x2000; break; }
            gCarSmokeDir[slot] |= 0x2000; v->flags |= 1;
            break;
        }
        if (type < 4) {
            scale = -2.0f - (f32)i * spacing;
            pos[0] = car->forward[0] * scale;
            pos[1] = car->forward[1] * scale;
            pos[2] = car->forward[2] * scale;
            pos[0] = pos[0] + car->wheel[type][0];
            pos[1] = pos[1] + car->wheel[type][1];
            pos[2] = pos[2] + car->wheel[type][2];
            pos[0] = pos[0] + car->right[0] * (Random(2.0f) - 1.0f);
            
            pos[1] = pos[1] + car->right[1] * (Random(2.0f) - 1.0f);
            
            pos[2] = pos[2] + car->right[2] * (Random(2.0f) - 1.0f);
             pos[1] += PC_MODEL_SUSPENSION(slot)[type]; 
            ObjInstanceList[v->handle.word].scaleA = Random(0.75f) + 0.25f;
        }
        if (type >= 6) {
            pos[0] = car->pos[0];
            pos[1] = car->pos[1];
            pos[2] = car->pos[2];
            pos[0] = pos[0] + car->up[0] * offset[1];
            pos[1] = pos[1] + car->up[1] * offset[1];
            pos[2] = pos[2] + car->up[2] * offset[1];
            pos[0] = pos[0] + car->forward[0] * offset[2];
            pos[1] = pos[1] + car->forward[1] * offset[2];
            pos[2] = pos[2] + car->forward[2] * offset[2];
            pos[0] = pos[0] + C_80123994 * car->forward[0];
            pos[1] = pos[1] + C_80123994 * car->forward[1];
            pos[2] = pos[2] + C_80123994 * car->forward[2];
            if (type == 6) {
                pos[0] = pos[0] + car->right[0] * offset[0];
                pos[1] = pos[1] + car->right[1] * offset[0];
                pos[2] = pos[2] + car->right[2] * offset[0];
                pos[0] = pos[0] + C_80123998 * car->right[0];
                pos[1] = pos[1] + C_80123998 * car->right[1];
                pos[2] = pos[2] + C_80123998 * car->right[2];
            } else {
                pos[0] = pos[0] + car->right[0] * offset[0];
                pos[1] = pos[1] + car->right[1] * offset[0];
                pos[2] = pos[2] + car->right[2] * offset[0];
                pos[0] = pos[0] + C_8012399C * car->right[0];
                pos[1] = pos[1] + C_8012399C * car->right[1];
                pos[2] = pos[2] + C_8012399C * car->right[2];
            }
            pos[0] = pos[0] + 0.5f * car->up[0];
            pos[1] = pos[1] + 0.5f * car->up[1];
            pos[2] = pos[2] + 0.5f * car->up[2];
            if (first) {
                if (type == 6) {
                    if (!(gCarSmokeDir[slot] & 0x4000)) gCarSmokeDir[slot] |= 0x4000;
                    else { previous[0] = gCarWingSmokePos[slot][0]; previous[1] = gCarWingSmokePos[slot][1]; previous[2] = gCarWingSmokePos[slot][2]; }
                    gCarWingSmokePos[slot][0] = pos[0]; gCarWingSmokePos[slot][1] = pos[1]; gCarWingSmokePos[slot][2] = pos[2];
                } else if (type == 7) {
                    if (!(gCarSmokeDir[slot] & 0x8000)) gCarSmokeDir[slot] |= 0x8000;
                    else { previous[0] = gCarWingSmokePos[slot][3]; previous[1] = gCarWingSmokePos[slot][4]; previous[2] = gCarWingSmokePos[slot][5]; }
                    gCarWingSmokePos[slot][3] = pos[0]; gCarWingSmokePos[slot][4] = pos[1]; gCarWingSmokePos[slot][5] = pos[2];
                }
            } else {
                delta[0] = previous[0] - pos[0];
                delta[1] = previous[1] - pos[1];
                delta[2] = previous[2] - pos[2];
                distance = NormalVector(delta);
                if (distance != 0.0f) {
                    scale = (f32)i * (distance / (f32)count);
                    pos[0] = pos[0] + scale * delta[0];
                    pos[1] = pos[1] + scale * delta[1];
                    pos[2] = pos[2] + scale * delta[2];
                }
            }
            ObjInstanceList[v->handle.word].scaleA = Random(C_801239A0) + C_801239A4;
        }
         MBOX_ShowObject(v->handle.word, first = 0, 15); 
        if (car->viewport >= 0 && (car->visibility == 0 || car->visibility == 1))
            MBOX_HideObject(v->handle.word, 0, 1 << car->viewport);
        if (type >= 6) {
            switch (gCarWings_players[slot * 13 + model->type]) {
            case 0: { s16 h = v->handle.word; ObjInstanceList[h].rgba.word = color1; } break;
            case 1: { s16 h = v->handle.word; ObjInstanceList[h].rgba.word = color2; } break;
            case 2: { s16 h = v->handle.word; ObjInstanceList[h].rgba.word = color3; } break;
            }
        } else if (type == 4) {
            ObjInstanceList[v->handle.word].rgba.byte[0] = gCarBlastSmokeClr[slot];
            ObjInstanceList[v->handle.word].rgba.byte[1] = gCarBlastSmokeClr[slot];
            ObjInstanceList[v->handle.word].rgba.byte[2] = gCarBlastSmokeClr[slot];
            ObjInstanceList[v->handle.word].rgba.byte[3] = 255;
            gCarBlastSmokeClr[slot] -= 3;
            if (gCarBlastSmokeClr[slot] < 53) gCarBlastSmokeClr[slot] = 53;
        } else if (type == 5) { s16 h = v->handle.word; ObjInstanceList[h].rgba.word = color0; }
        else if (surface == 1) { s16 h = v->handle.word; ObjInstanceList[h].rgba.word = color5; }
        else { s16 h = v->handle.word; ObjInstanceList[h].rgba.word = color4; }
        v->pos[0] = pos[0];
        v->pos[1] = pos[1];
        v->pos[2] = pos[2];
        i++;
    } while (count != i);
}

static Car *get_car(s16 slot) { return PC_CAR(slot); }
static s16 get_mph(Car *c) { return c->mph >> 2; }
