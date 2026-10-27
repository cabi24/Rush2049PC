/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Image B: initialize four pools, resolve model/texture names, reset effects.
 * Strict standalone match: 140 words / 560 bytes, stock IDO 5.3.
 * The indexed texture loops preserve the known-global array provenance in UGEN,
 * so as1 can schedule the fifth outgoing argument before the name load. The
 * prior pointer-cursor form loses that no-alias fact and differs at six words.
 * No padding, unused locals, extra reads, or ABI shaping is required.
 * Existing lookup interfaces and PlayerEffect layout follow the existing
 * runtime_b_initializer_20261006; whole-function arcade ancestry
 * is unestablished. The inherited volatile count view is noncausal here.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef int s32;
typedef struct NameEntry NameEntry;
extern void struct_fields_init(void *, void *, s32, s32, u8);
extern s32 string_copy_format(char *, s8, s8, s8);
extern NameEntry *func_800B24EC(char *, s16 *, s8, s8, s32);
extern void func_8038D1A8(void);
/* Same qualified view as both accepted lookup implementations; noncausal here. */
extern volatile u8 D_80140BDC;
extern s16 D_80394F88;
#ifdef TARGET_PC
/* PC: the list headers have the StatLL host layout (40 bytes) and the element arrays their elements' host layout,
 * shots.c's Shot (104 bytes on the N64, 120 here), HitPoly (8, 16), ShotObj (60, 72) and ShotCase (72, 80), which
 * are linked through their first pointer. These declarations give them that storage (gen_data), and the lists the
 * host element size (docs/port/POINTERS.md 9). */
typedef struct { u8 dbl; s32 numElements, elementSize; void *list, *head, *store; } PcStatLL;
typedef struct { void *next; s8 slot, slotDamaged, type, flags; float lifeTime, incTime, radius, vel[3], pos[3], old[3];
    float uvs[3][3]; void *streak, *obj, *obj2; } PcShot;
typedef struct { void *next, *poly; } PcHitPoly;
typedef struct { void *next; s32 mbIdx; float mat[12]; void *shotInfo; } PcShotObj;
typedef struct { void *next; s32 mbIdx; float mat[12]; float time, vel[3]; } PcShotCase;
extern PcStatLL D_8039A520[1], D_8039A5F8[1], D_80394F70[1], D_8039AE80[1];
extern PcShot D_80399B60[24];
extern PcHitPoly D_8039A538[24];
extern PcShotObj D_8039AE98[24];
extern PcShotCase D_8039A610[30];
#define ELEMENT_SIZE(n64, type) sizeof(type)
#else
extern u8 D_8039A520[], D_80399B60[], D_8039A5F8[], D_8039A538[];
extern u8 D_80394F70[], D_8039AE98[], D_8039AE80[], D_8039A610[];
#define ELEMENT_SIZE(n64, type) n64
#endif
extern char *D_803942EC[10], *D_80394314[7];
extern char *D_80394330[5], *D_80394344[5];
extern s32 D_80399B18[10], D_80399B40[7];
extern s16 D_80399AF8[5], D_80399B08[5];
typedef struct PlayerEffect { s32 handle; float transform[12]; } PlayerEffect;
extern PlayerEffect D_80394F90[4];
void func_803908D0(void)
{
    s32 *model;
    char **name;
    s32 i;
    D_80394F88 = 0;
    struct_fields_init(D_8039A520, D_80399B60, ELEMENT_SIZE(104, PcShot), 24, 0);
    struct_fields_init(D_8039A5F8, D_8039A538, ELEMENT_SIZE(8, PcHitPoly), 24, 0);
    struct_fields_init(D_80394F70, D_8039AE98, ELEMENT_SIZE(60, PcShotObj), 24, 0);
    struct_fields_init(D_8039AE80, D_8039A610, ELEMENT_SIZE(72, PcShotCase), 30, 0);
    for (name = D_803942EC, model = D_80399B18; model < D_80399B18 + 10; model++, name++) {
        *model = string_copy_format(*name, 0, D_80140BDC - 1, 0);
    }
    for (name = D_80394314, model = D_80399B40; model < D_80399B40 + 7; model++, name++) {
        *model = string_copy_format(*name, 0, D_80140BDC - 1, 0);
    }
    for (i = 0; i < 5; i++) {
        func_800B24EC(D_80394330[i], &D_80399AF8[i], 0, D_80140BDC - 1, 1);
    }
    for (i = 0; i < 5; i++) {
        func_800B24EC(D_80394344[i], &D_80399B08[i], 0, D_80140BDC - 1, 1);
    }
    for (i = 0; i < 4; i++) {
        D_80394F90[i].handle = -1;
    }
    func_8038D1A8();
}
