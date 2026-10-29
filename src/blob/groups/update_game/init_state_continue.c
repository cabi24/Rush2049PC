/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* init_state_continue (0x800FAF6C) = MAT3 game.c _CalcPolePositions (GC 0x8001581C, size 0x208).
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * : no stand-in any more -- its real caller countdown (= _UpdateGame) is in this group
 * (EQUAL in the whole-program unit and in the group with countdown as caller). Its other caller,
 * display_list_flush (= PrePlay), is still retail asm and calls it at its fixed address.
 *
 * init_state_continue (0x800FAF6C, 178 w, frameless, writes s0-s8 unsaved: IPA-internal): fill the six
 * 8-byte race slot configs at D_80153E88. The extra-player count is min(D_80142724, 6 - D_8014A108), zero in
 * modes 4/5/6; D_801543CA = D_8014A108 + extra. Mode 3 with D_80154628 > 0 takes player ids from D_80154450[]
 * (76-byte records, slots 0/1 swapped by D_801543D4); human slots get kind 6, others kind 0 with either a
 * sequential id or (mode 3) a random id not already used (retry until unique); empty slots kind 7.
 * (Semantics: .)
 *
 * Code identical with a stand-in caller; kept it gets a standard frame (131/178 differ), so it must
 * stay internal with countdown as its C caller.
 *
 * What closed it (from 50 rows):
 *  - the random pick is the locked Random() func_8008B2E4 (func_8008B2E4.c in this group, byte-identical
 *    copy of src/blob/func_8008B2E4.c), umerge-inlined with its static rand(): its redundant `& 0x7FFF` and
 * named `rannum` give retail's temp ring in the LCG block (own frand had the rand value in v0);
 *  - the retry loop is `while (1) { ...; for (prior = 0; ...) ...; if (slot == prior) break; }`: retail's
 *    `beql a1,v1,exit; b top; move v1,zero` latch; `slot == prior` gives the `a1,v1` operand order;
 *  - `extern volatile s16 D_801543CA` (as in several locked files): retail reloads it at the retry-loop top;
 *    non-volatile, PRE moves the load to the latch and the address web drops to s1 (90 rows);
 *  - the mode 4/5/6 test as an if-chain (`bnel a0,v0` order; with the other changes no `ra` frame any more);
 *  - `config = D_80153E88` in the for-init after `slot = 0` (as1 order of the `addiu a2`).
 * Disclosed shaping: `volatile` on D_801543CA (declaration used by locked code); no unused locals, no
 * compiled-out reads.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Config {
    /* 0 */ s8 type;
    /* 1 */ u8 pad1[4];
    /* 5 */ u8 selection;
    /* 6 */ u8 flags;
    /* 7 */ u8 kind;
} Config;

typedef struct Player {
    /* 0 */ u8 pad0;
    /* 1 */ u8 id;
    /* 2 */ u8 pad2[74];
} Player; /* 0x4C */

extern s16 D_8014A108;
extern s16 D_80142724;
extern volatile s16 D_801543CA;
extern s32 D_8014A110;
extern s8 D_80154628;
extern s8 D_8014978C;
extern u8 D_801543D4;
extern Player D_80154450[];
#ifdef TARGET_PC
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Config D_80153E88[];
#define PC_LINK Config
#define PC_LINKS D_80153E88
#define PC_LINK_PLACE(i) D_80153E88[i].selection
#define PC_LINKP_TRACK(p) p->type
#define PC_LINKP_PLACE(p) p->selection
#define PC_LINKP_FLAGS(p) p->flags
#define PC_LINKP_OWNER(p) p->kind
#endif

#ifdef TARGET_PC
/* PC: a race holds up to the setting's cars (pc_game_types.h, docs/port/MORECARS.md 3.1). The race's start latches
 * the setting; the results screen calls this again for the cars that race on behind it (display_list_flush, state
 * 0x2000000), which keep the race's count. The drones fill up to the race's cars, but the circuit stays the N64's 6
 * (MORECARS.md 3.3: its standings, points and records are six). Every slot a PC race can use is filled, so the slots
 * after this race's cars are empty (owner 7, flags 0), whatever an earlier race of more cars left in them. */
extern u32 state_word_a;
#define PC_LATCH_CARS() if (!(state_word_a & 0x2000000)) pc_race_cars_begin();
#define RACE_CARS (D_8014A110 == 3 ? PC_N64_CARS : PC_RACE_CARS)
#define LINK_SLOTS PC_MAX_LINKS
#else
#define PC_LATCH_CARS()
#define RACE_CARS 6
#define LINK_SLOTS 6
#endif

f32 func_8008B2E4(f32 max);
void CalcPolePositions(void)
{
    s32 extra;
    s32 slot;
    s32 prior;
    u8 next = 0;
    PC_LINK *config;

    PC_LATCH_CARS() extra = D_80142724;
    if (RACE_CARS - D_8014A108 < extra) {
        extra = RACE_CARS - D_8014A108;
    }
    if (D_8014A110 == 4 || D_8014A110 == 5 || D_8014A110 == 6) {
        extra = 0;
    }
    D_801543CA = D_8014A108 + extra;
    for (slot = 0, config = PC_LINKS; slot < LINK_SLOTS; slot++, config++) {
        if (D_8014A110 == 3 && slot < D_801543CA && D_80154628 > 0) {
            PC_LINKP_PLACE(config) = D_80154450[(slot == 0) ? D_801543D4 : ((slot == 1) ? D_801543D4 ^ 1 : slot)].id;
            PC_LINKP_FLAGS(config) = 176;
            if (slot < D_8014A108) {
                PC_LINKP_OWNER(config) = 6;
            } else {
                PC_LINKP_OWNER(config) = 0;
            }
        } else if (slot < D_8014A108) {
            PC_LINKP_PLACE(config) = extra++;
            PC_LINKP_OWNER(config) = 6;
            PC_LINKP_FLAGS(config) = 176;
        } else if (slot < D_801543CA) {
            if (D_8014A110 == 3) {
                while (1) {
                    PC_LINKP_PLACE(config) = func_8008B2E4(D_801543CA);
                    for (prior = 0; prior < slot; prior++) {
                        if (PC_LINKP_PLACE(config) == PC_LINK_PLACE(prior)) {
                            break;
                        }
                    }
                    if (slot == prior) {
                        break;
                    }
                }
            } else {
                PC_LINKP_PLACE(config) = next++;
            }
            PC_LINKP_OWNER(config) = 0;
            PC_LINKP_FLAGS(config) = 176;
        } else {
            PC_LINKP_OWNER(config) = 7;
            PC_LINKP_FLAGS(config) = 0;
        }
        PC_LINKP_TRACK(config) = D_8014978C;
    }
}
