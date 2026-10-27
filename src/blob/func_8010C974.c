/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_8010C974 (0x8010C974, 2,636 B) = _AnimateMine@targets.c (MAT3 0x8004D428), the weapon-mine visual callback:
 * op 0 releases the environ; otherwise (unpaused) the first frame retires the owner's oldest mine when it already
 * has three (age 121), takes a free slot, aligns the mine to the road (RoadPosition = camera_trigger_check), builds
 * the base object (white, N64 flag 0x2000 on the mine model), the team-coloured glow rings and their matrices;
 * every frame it flashes the base texture (0.25 s), grows/fades the two glow rings (scale += 0.25 every 1/30 s,
 * alpha -8 from step 20, the other ring restarts at step 25, reset at step 45), and after 120 s or when hit removes
 * the always-collide links, the slot, the objects, damages (ProximityDamage) and blasts.
 * , from mine/best.c (MAT3 port; 49 rows). MATCH: the scorer fn MATCH (own.rodata verified),
 * whole-program unit EQUAL (kept).
 * What closed it (all traced):
 *  - Retail keeps the oldest-mine index, the uvs-copy inner index and the scale loop's OUTER index in one variable
 *    j (a1 in all three loops) and the scale loop's inner index in k (v1): MAT3 port with j outer / k inner.
 *  - DISCLOSED SHAPING: `j = -1;` after the uvs copy is a dead store (no code). Without it the unrolled search's
 *    `j = 0` is partially available at the scale loop's `j = 0`, uopt's PRE moves that store into the math_utility
 *    call block (pretrace: str j,0 DELETE 58 INSERT 57) and j is spilled around the call (661 words). Any non-zero
 *    store to j after the uvs copy and before the glow loop gives the same code.
 * - DISCLOSED SHAPING: ZOID_ObjectFlags / ZOID_SetObjectFlags each reach the object through one more
 *    inlined level (_ObjFlags / _SetObjFlags): two 8-byte umerge inline reserves = retail's 16-byte frame gap
 *    (392). Which single-call helper chain carries the 16 bytes is not determined by the bytes.
 *  - mat[1] position adds written `x = x + uv` (retail add.s operand order) with the [1] and [2] adds on one source
 *    line (as1 line tie: retail loads mat[1][1][2] before the [1] reload); the scale loop's inner body in braces
 *    (as1 delay-slot choice of the inner loop).
 *  - Locals i, j, k, t, m are the five slots above clr; no unused locals.
 */
typedef signed char s8; typedef unsigned char u8;
typedef signed short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32; typedef float f32;
typedef union Color {struct {u8 r,g,b,a;} c;u32 word;} Color;
typedef struct MineData {
 s32 base,ring[2]; f32 timer; f32 mat[2][4][3];
 f32 elapsed[2]; s8 step[2]; u8 alpha[2]; f32 scale[2]; s16 phase,active;
} MineData;
typedef struct Target {
 struct Target *next;u8 flags,pad5[7];s32 model;s16 info;u8 pad18[2];
 f32 mat[4][3],position[3];s16 kind;u8 pad82[10];s8 car;u8 pad93[3];
 s32 sound;s8 sound_state,owner;u8 pad102[2];f32 age;MineData *data;
} Target;
typedef struct Visual {struct Visual *next;s16 index,model,owner;u8 pad10[2];Target *data;f32 time;void (*callback)(struct Visual*,s16);} Visual;
#ifdef TARGET_PC
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). The colour is a member of its own:
 * a union of the eight pointer slots and the colour would be padded to 72 bytes. */
typedef struct Object68 {u32 flags;s32 w4;void *owner;f32 scaleA,scaleB;u16 id;s16 link[3];void *slots[8];Color color;s32 w64;} Object68;
#else
typedef struct Object68 {u32 flags;s32 w4;void *owner;f32 scaleA,scaleB;u16 id;s16 link[3];union {s32 words[9];struct {s32 prefix[8];Color color;} view;} body;s32 w64;} Object68;
#endif
typedef struct Texture36 {u8 bytes[36];} Texture36;
typedef struct TextureBank {Texture36 *base;u32 other;} TextureBank;
typedef struct MineSlots {Target *target[4];s32 count;} MineSlots;
typedef struct Link {struct Link *next;Target *target;} Link;
#ifdef TARGET_PC
/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct Pool {u8 doubly;s32 count,size;void *mem;Link *active,*free;} Pool;
#else
typedef struct Pool {u8 doubly,pad1[15];Link *active,*free;} Pool;
#endif
extern s32 D_801170FC,D_80399B2C,D_80399B30;
extern volatile f32 D_8002EB94;
extern MineSlots D_80151528[];
extern Object68 D_8012E700[];
extern TextureBank D_80151AE8[];
extern u16 D_80399AF8[];
extern s8 gCarTeam[];
extern u8 D_80121D54[][3];
extern Pool D_80151AA8,D_80143FC8;
extern void entity_transform_apply(Visual *,s32);
extern void *camera_trigger_check(f32 *,f32 *,f32 (*)[3]);
extern s16 func_8008E26C(s32,void *,s16,s32);
extern void func_8008D870(s16,Texture36 *,s32);
extern void math_utility(void *,void *);
extern void model_data_load(s32,s32,u32),model_transform_setup(s32,s32,u32);
extern void func_800AFA84(Pool *,void *);
extern void func_8038D798(f32 *,f32 *,s32,f32,s32);
extern void save_write_data(f32 *,s32,f32,s32);
extern void entity_spawn_callback(s16,s32,s32);
static s16 MBOX_NewObject(s32 obj,void *mat,s32 parent,u32 flags){s16 dlidx;dlidx=func_8008E26C(obj,mat,parent,flags);return dlidx;}
#ifdef TARGET_PC
static void ZOID_SetColor(s16 objnum,Color *col){D_8012E700[objnum].color.word=col->word;}
static Color *ZOID_GetColor(s16 objnum){return &D_8012E700[objnum].color;}
#else
static void ZOID_SetColor(s16 objnum,Color *col){D_8012E700[objnum].body.view.color.word=col->word;}
static Color *ZOID_GetColor(s16 objnum){return &D_8012E700[objnum].body.view.color;}
#endif
static u32 _ObjFlags(s16 objnum){return D_8012E700[objnum].flags;}
static u32 ZOID_ObjectFlags(s16 objnum){return _ObjFlags(objnum);}
static void _SetObjFlags(s32 objnum,u32 flags){D_8012E700[objnum].flags=flags;}
static void ZOID_SetObjectFlags(s32 objnum,u32 flags){_SetObjFlags(objnum,flags);}
static void ZOID_RemoveObjectTree(s16 objnum,s32 removeChildren){entity_spawn_callback(objnum,removeChildren,0);}
static void ZOID_RemoveObject(s16 objnum){ZOID_RemoveObjectTree(objnum,0);}
void func_8010C974(Visual *v, s16 op)
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
        entity_transform_apply(v, 1);
        return;
    }
    if (D_801170FC != 0) return;
    t = v->data;
    m = t->data;
    if (!(t->flags & 1)) {
        if (D_80151528[t->owner].count == 3) {
            oldT = 0.0f;
            j = -1;
            for (i = 0; i < 4; i++) {
                if (D_80151528[t->owner].target[i] && D_80151528[t->owner].target[i]->age > oldT) {
                    j = i;
                    oldT = D_80151528[t->owner].target[i]->age;
                }
            }
            if (j != -1) D_80151528[t->owner].target[j]->age = 121.0f;
        }
        for (i = 0; i < 4; i++) {
            if (!D_80151528[t->owner].target[i]) {
                D_80151528[t->owner].target[i] = t;
                D_80151528[t->owner].count++;
                break;
            }
        }
        if (camera_trigger_check(t->position, opos, uvs)) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) t->mat[i][j] = uvs[i][j];
            }
        }
        j = -1;
        m->base = MBOX_NewObject(D_80399B2C, 0, t->model, 0x2080);
        clr.c.r = 255;
        clr.c.g = 255;
        clr.c.b = 255;
        clr.c.a = 255;
        ZOID_SetColor(m->base, &clr);
        ZOID_SetObjectFlags(t->model, ZOID_ObjectFlags(t->model) | 0x2000);
        ZOID_SetColor(t->model, &clr);
        func_8008D870(m->base, &D_80151AE8[D_80399AF8[4] >> 10].base[D_80399AF8[4] & 1023], -1);
        m->timer = 0.0f;
        m->phase = 0;
        clrPtr = &clr;
        clrPtr->c.r = D_80121D54[gCarTeam[t->owner]][0];
        clrPtr->c.g = D_80121D54[gCarTeam[t->owner]][1];
        clrPtr->c.b = D_80121D54[gCarTeam[t->owner]][2];
        math_utility(t->mat, m->mat[0]);
        math_utility(t->mat, m->mat[1]);
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
        m->ring[0] = MBOX_NewObject(D_80399B30, m->mat[0], -1, 0x42000);
        ZOID_SetColor(m->ring[0], &clr);
        m->ring[1] = MBOX_NewObject(D_80399B30, m->mat[1], -1, 0x42000);
        ZOID_SetColor(m->ring[1], &clr);
        m->alpha[0] = 255;
        m->alpha[1] = 255;
        m->step[0] = 0;
        m->step[1] = -1;
        m->scale[0] = 1.0f;
        m->scale[1] = 1.0f;
        m->elapsed[0] = 0.0f;
        m->elapsed[1] = 0.0f;
        model_data_load(m->ring[1], 0, 15);
        t->flags |= 3;
    }
    m->timer += D_8002EB94;
    if (m->timer >= 0.25f) {
        if (m->phase) {
            m->phase = 0;
            func_8008D870(m->base, &D_80151AE8[D_80399AF8[4] >> 10].base[D_80399AF8[4] & 1023], -1);
        } else {
            m->phase = 1;
            func_8008D870(m->base, &D_80151AE8[D_80399AF8[gCarTeam[t->owner]] >> 10].base[D_80399AF8[gCarTeam[t->owner]] & 1023], -1);
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
            math_utility(t->mat, m->mat[i]);
            m->step[i] = -1;
            model_data_load(m->ring[i], 0, 15);
            continue;
        }
        if (m->elapsed[i] >= 0.0333333f) {
            math_utility(t->mat, m->mat[i]);
            for (j = 0; j < 3; j++) {
                for (k = 0; k < 3; k++) {
                    m->mat[i][j][k] *= m->scale[i];
                }
            }
            m->scale[i] += 0.25f;
            m->elapsed[i] = 0.0f;
            m->step[i]++;
        }
        m->elapsed[i] += D_8002EB94;
        if (m->step[i] >= 20) {
            if (m->alpha[i] > 8) m->alpha[i] -= 8;
            clrPtr = ZOID_GetColor(m->ring[i]);
            clrPtr->c.a = m->alpha[i];
        }
        if (m->step[i] >= 25) {
            j = !i;
            if (m->step[j] < 0) {
                m->step[j] = 0;
                model_transform_setup(m->ring[j], 0, 15);
            }
        }
    }
    t->age += D_8002EB94;
    if (!(t->flags & 2) || (t->flags & 4) || t->age > 120.0f) {
        acol = D_80151AA8.active;
        while (acol) {
            acolnext = acol->next;
            if (acol->target == t) func_800AFA84(&D_80151AA8, acol);
            acol = acolnext;
        }
        for (i = 0; i < 4; i++) {
            if (D_80151528[t->owner].target[i] == t) {
                D_80151528[t->owner].target[i] = 0;
                D_80151528[t->owner].count--;
                break;
            }
        }
        if (t->age > 120.0f) func_8038D798(t->position, t->position, t->owner, 400.0f, 1600);
        save_write_data(t->position, 0, 0.4f, 1);
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
        func_800AFA84(&D_80143FC8, t);
        entity_transform_apply(v, 1);
    }
}
