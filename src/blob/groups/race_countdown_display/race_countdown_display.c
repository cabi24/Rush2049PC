/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

/* Accessed 0x7EC prefix of the authentic 0x808 model record. This function
 * receives one pointer; it never uses sizeof(Model) or model-array indexing. */
typedef struct Model {
    u8 pad000[0x7C6];
    s16 slot;
    u8 pad7C8[0x7D4 - 0x7C8];
    u32 flags;
    u8 pad7D8[0x7DC - 0x7D8];
    s8 lap_enabled;
    u8 pad7DD[0x7E2 - 0x7DD];
    s16 last_cp;
    s16 next_cp;
    u8 pad7E6[2];
    s8 laps;
    u8 pad7E9;
    s8 finish_a;
    s8 finish_b;
} Model;

typedef struct GameCar {
    u8 pad000[0xEE];
    s8 place;
    s8 place_locked;
#ifdef TARGET_PC
    u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21];
#else
    u8 pad0F0[0x308 - 0xF0];
#endif
    s8 finish_a;
    s8 finish_b;
    u8 pad30A[2];
    f32 finish_timer;
    u8 pad310;
    s8 flags311;
#ifdef TARGET_PC
    u8 pad312[0x380 - 0x312]; void *input; u8 pad312_384[0x3B8 - 0x384];
#else
    u8 pad312[0x3B8 - 0x312];
#endif
} GameCar;

typedef struct Player {
    u8 pad0[7];
    u8 type;
} Player;

/* Accessed ten-byte prefix of the checkpoint header; no array indexing. */
typedef struct Track {
    s16 unknown0;
    s16 loop_start;
    s16 finish_line;
    s16 before_finish;
    s16 count;
} Track;

extern Track D_80151CE8;
#ifdef TARGET_PC
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Player D_80153E88[];
#define PC_LINK Player
#define PC_LINKS D_80153E88
#define PC_LINKP_OWNER(p) p->type
#endif
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern GameCar D_80152818[];
#define PC_CAR(i) &D_80152818[i]
#endif
extern s32 D_8014A110;
extern u32 D_801174B4;
extern s16 D_80152734;
extern s8 D_80152014;
extern s8 D_80152015;
extern s8 D_80110680[];
extern f32 D_80110668[];
extern volatile f32 D_8002EB90;
void func_800D2458(s32 player, f32 time);
void func_800D2054(s32 player, f32 time);
void car_setup_confirm(s32 player, f32 time);
static s32 func_800B61A8(s32 code, s32 player, s32 value, u8 mode);
u32 car_stats_display(s32 type, f32 delay, s32 callback, s32 data,
                      s32 nargs, ...);

s32 func_800B930C(s32 index);

void race_countdown_display(Model *m, f32 time)
{
    s32 old_remaining;
    s32 player;
    PC_LINK *link;
    GameCar *gc;
    s32 completed_lap;

    completed_lap = 0;
    player = m->slot;
    m->last_cp = m->next_cp;
    m->next_cp = func_800B930C(m->last_cp);
    if (m->last_cp == D_80151CE8.finish_line && m->lap_enabled &&
        D_8014A110 != 1 && !(D_801174B4 & 8)) {
        m->laps++;
        completed_lap = 1;
    }
    if (!m->lap_enabled && m->last_cp == D_80151CE8.before_finish) {
        m->lap_enabled = 1;
    }
    link = &PC_LINKS[player];
    gc = PC_CAR(player);
    if (PC_LINKP_OWNER(link) == 6) {
        if (m->last_cp == D_80151CE8.finish_line && !(D_801174B4 & 8)) {
            func_800D2458(player, time);
        }
        D_80152015 = m->next_cp == D_80151CE8.finish_line &&
            m->laps + 1 == D_80152734;
    }
    if (completed_lap) {
        func_800D2054(m->slot, time);
        if (m->laps == D_80152734 && !gc->place_locked) {
            car_setup_confirm(m->slot, time);
        }
        if (PC_LINKP_OWNER(link) == 6 && (D_8014A110 != 2 || player == 0)) {
            old_remaining = D_80152014;
            D_80152014 = D_80152014 < D_80152734 - m->laps ?
                D_80152014 : D_80152734 - m->laps;
            if (m->laps == D_80152734 && gc->place_locked != -1) {
                D_80110680[player] = 1;
                D_80110668[player] = D_8002EB90;
                switch (gc->place) {
                case 0:
                    func_800B61A8(64, player, 1, 1);
                    break;
                case 1:
                    func_800B61A8(68, player, 1, 1);
                    break;
                case 2:
                    func_800B61A8(67, player, 1, 1);
                    break;
                default:
                    func_800B61A8(65, player, 1, 1);
                    break;
                }
                gc->finish_a = 1;
                gc->finish_b = 1;
                gc->flags311 |= 1;
                gc->finish_timer = 0.0f;
                m->finish_a = 1;
                m->finish_b = 1;
                m->flags &= ~8;
            } else if (old_remaining != D_80152014) {
                if (D_80152014 == 1) {
                    func_800B61A8(74, player, 1, 1);
                } else {
                    car_stats_display(0, 2.0f, 1, 0, 2, D_80152014 + 79, 80);
                }
            }
        }
    }
}
extern signed char D_8010FFC0;
s32 entity_flags_apply(s32, s32, s32, u8);
static s32 func_800B61A8(s32 a0, s32 a1, s32 a2, u8 a3)
{
  if (D_8010FFC0 == 0) {
    return -1;
  }
  if (a0 == -1) {
    return -1;
  }
  return entity_flags_apply(a0, a1, a2, a3);
}
