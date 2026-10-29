/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * cpak_read (0x800B0180, 212 words): per-car visual-effects update. N64 descendant of arcade
 * game/visuals.c UpdateVisuals(slot) (the name is a historical label):
 *   - gGoodFrameRate (D_80143F14) = frame time in microseconds below a budget: 36000 us when the video
 *     rate D_8002AFB4 is 50 Hz, else 45000 us with 2+ players (active_player_count), else 30000 us.
 *     The time is OS_CYCLES_TO_USEC of the CPU-count delta D_80124F70 (count * 64 / 3000, u64).
 *   - car->appearance & 0x60 (smoke) -> StartSmoke(slot, 4, 0, 0) (= entity_spawn_init).
 *   - per tire: model[slot].sviscode[t] == 2 (water) -> StartSmoke(slot, t, 1, 0); else the tire's
 *     appearance smoke bit (0x40000, 0x80000, 0x10000, 0x20000) -> StartSmoke(slot, t, 0, 0).
 *   - the 21 Visual controllers at car+272 (24-byte records, func at +20): (*v->func)(v, 1).
 * - cpak_init(slot) = the DoSkid driver (locked group skid_marks, defined `void cpak_init(s32)`).
 * Unlike the arcade the N64 does not gate the smoke on gGoodFrameRate and has no frames % 4 phase.
 * Devices / notes (disclosed):
 *   - D_80124F70 is `volatile` (bss count delta, read once). Retail addresses it `lui/addiu/lw 0(t9)`,
 *     which IDO emits only for a volatile access; non-volatile gives `lw %lo(t9)` (89 words off).
 *   - cpak_init is declared with its real definition's parameter type s32 (the locked group defines
 *     `void cpak_init(s32 slot)`); retail passes s1 without re-narrowing.
 *   - `D_8002AFB4 == 50.0f` operand order puts 50.0 in $f4 as retail; `50.0f == D` swaps them.
 *   - NATURAL FORM: the time is written `(u64)D_80124F70 * 64 / 3000`. IDO lowers that to calls of the
 *     ultralib runtime helpers __ll_mul / __ull_div / __ull_to_f (libc/ll.c, llcvt.c). The repo names
 * those static functions by their ultralib names since (0x8000DA58 / 0x8000D958
 *     0x80009BC8; libgcc names __muldi3 / __udivdi3 / __floatundisf before), so the link resolves them.
 * Before that rename this delivered the same bytes with the three calls written out.
 */
typedef signed char s8; typedef unsigned char u8;
typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
typedef unsigned long long u64;
typedef struct Visual { struct Visual *next; s32 index; s32 objnum; u32 slot; u32 data; void (*func)(struct Visual *v, s16 op); } Visual;
#ifdef TARGET_PC
typedef struct Car { u8 pad0[232]; u32 appearance; u8 pad236[36]; Visual visuals[21]; u8 pad776[0x380 - 0x308]; void *input; u8 pad776_384[0x3B8 - 0x384]; } Car;
#else
typedef struct Car { u8 pad0[232]; u32 appearance; u8 pad236[36]; Visual visuals[21]; u8 pad776[176]; } Car;
#endif
typedef struct Model { u8 pad0[1564]; u16 sviscode[4]; u8 pad1572[484]; } Model;
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Car D_80152818[];
#define PC_CAR(i) &D_80152818[i]
#endif
#ifdef TARGET_PC
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Model D_8014A250[];
#define PC_MODEL_VISCODE(i) D_8014A250[i].sviscode
#endif
extern volatile u32 D_80124F70;
extern f32 D_8002AFB4;
extern s16 active_player_count;
extern u8 D_80143F14;
void entity_spawn_init(s16 slot, u32 type, s32 fast, f32 *offset);
void cpak_init(s32 slot);

void cpak_read(s16 slot)
{
    Car *car = PC_CAR(slot);
    Visual *v = &car->visuals[0];
    s16 i;
    f32 usec;

    usec = (u64)D_80124F70 * 64 / 3000;
    if (D_8002AFB4 == 50.0f) {
        D_80143F14 = usec < 36000.0f;
    } else if (active_player_count >= 2) {
        D_80143F14 = usec < 45000.0f;
    } else {
        D_80143F14 = usec < 30000.0f;
    }

    if (car->appearance & 0x60)
        entity_spawn_init(slot, 4, 0, 0);

    if (PC_MODEL_VISCODE(slot)[0] == 2)
        entity_spawn_init(slot, 0, 1, 0);
    else if (car->appearance & 0x40000)
        entity_spawn_init(slot, 0, 0, 0);
    if (PC_MODEL_VISCODE(slot)[1] == 2)
        entity_spawn_init(slot, 1, 1, 0);
    else if (car->appearance & 0x80000)
        entity_spawn_init(slot, 1, 0, 0);
    if (PC_MODEL_VISCODE(slot)[2] == 2)
        entity_spawn_init(slot, 2, 1, 0);
    else if (car->appearance & 0x10000)
        entity_spawn_init(slot, 2, 0, 0);
    if (PC_MODEL_VISCODE(slot)[3] == 2)
        entity_spawn_init(slot, 3, 1, 0);
    else if (car->appearance & 0x20000)
        entity_spawn_init(slot, 3, 0, 0);

    for (i = 0; i < 21; ++i, ++v)
        if (v->func)
            (*v->func)(v, 1);

    cpak_init(slot);
}
