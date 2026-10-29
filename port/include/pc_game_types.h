/* port/include/pc_game_types.h: game data the PC owns outright, with no N64 address (docs/port/DECOUPLE.md).
 * pc_prelude.h includes it, so every game C file sees it on TARGET_PC; the N64 build never does. Each item has one
 * PC declaration here, storage gen_data defines in data.c (port/data/decoupled.txt: outside the RDRAM window, zero
 * at startup), and accessors the game C reaches it through. A game file that uses an accessor defines its N64 side
 * itself, with its own view of the record, so the N64 tokens are the original ones:
 *     #ifndef TARGET_PC
 *     #define PC_LINK_OWNER(i) D_80153E88[i].owner
 *     #endif
 * No system headers, as for pc_prelude.h. */
#ifndef PC_GAME_TYPES_H
#define PC_GAME_TYPES_H

/* ---- The race roster: arcade gLink, N64 D_80153E88 (13 records of 8 bytes at 0x80153E88-0x80153EF0, BSS).
 * One record per car slot: who drives it and how it starts. The N64 game activates 6 slots (its MAX_LINKS: the
 * per-car arrays are 6 long), but LoadCars, sfx_position_3d and func_8039156C walk all 13, so the storage is never
 * shorter than that. PC_MAX_LINKS is the most cars a race may hold on the PC (12, twice the N64's 6); nothing
 * activates more than 6 yet. */
#ifndef PC_MAX_LINKS
#define PC_MAX_LINKS 12
#endif
#define PC_N64_LINK_SLOTS 13
#define PC_LINK_SLOTS (PC_MAX_LINKS > PC_N64_LINK_SLOTS ? PC_MAX_LINKS : PC_N64_LINK_SLOTS)

struct pc_link {
    unsigned char track;     /* +0 the track (CalcPolePositions, func_800EC190: D_8014978C) */
    unsigned char car;       /* +1 the car model, 0-12 (LoadCars, the car select) */
    unsigned char color[3];  /* +2 the car's three colours (the car select, LoadCars; the model record's +0x7CD) */
    unsigned char place;     /* +5 the start place (CalcPolePositions; the car record's place) */
    unsigned char flags;     /* +6 0x80 active (LINK_ACTIVE), 0x20 track and 0x10 car locked: 0xB0 in a race */
    unsigned char owner;     /* +7 0-5 a drone that node drives (0 here), 6 a human player, 7 an empty slot */
};
extern struct pc_link pc_links[PC_LINK_SLOTS];

/* The accessors: slot i's fields, the record type, the array, and the fields through a record pointer p. */
#define PC_LINK struct pc_link
#define PC_LINKS pc_links
#define PC_LINK_TRACK(i) (pc_links[i].track)
#define PC_LINK_CAR(i) (pc_links[i].car)
#define PC_LINK_COLOR0(i) (pc_links[i].color[0])
#define PC_LINK_COLOR1(i) (pc_links[i].color[1])
#define PC_LINK_COLOR2(i) (pc_links[i].color[2])
#define PC_LINK_PLACE(i) (pc_links[i].place)
#define PC_LINK_FLAGS(i) (pc_links[i].flags)
#define PC_LINK_OWNER(i) (pc_links[i].owner)
#define PC_LINKP_TRACK(p) ((p)->track)
#define PC_LINKP_PLACE(p) ((p)->place)
#define PC_LINKP_FLAGS(p) ((p)->flags)
#define PC_LINKP_OWNER(p) ((p)->owner)

/* ---- The model records: MAT3 model[], tModelData, N64 D_8014A250 (6 records of 0x808 bytes at 0x8014A250-
 * 0x8014D280, BSS). One record per car slot: its physics state, controls and race state. The record keeps its N64
 * layout on the PC (its pointer fields are u32 window addresses, docs/port/CARS.md 4), so its size is 0x808 here
 * too and code that steps through it by 0x808 still lands on the fields. Every walk of it stops at 6 (the cars in
 * the race, D_801543CA, D_80152744, MAX_LINKS), so the storage is PC_MODEL_SLOTS = the larger of 6 and
 * PC_MAX_LINKS; nothing activates more than 6 yet. Only the fields the game reaches by index (and the names inside
 * the array: D_8014AA16 is [0].slot) are named here; code that takes a record's address (PC_MODEL) reads it
 * through its own view, which view_audit.py checks against the same layout. */
#define PC_N64_MODEL_SLOTS 6
#define PC_MODEL_SLOTS (PC_MAX_LINKS > PC_N64_MODEL_SLOTS ? PC_MAX_LINKS : PC_N64_MODEL_SLOTS)

struct pc_model {
    unsigned int car;                   /* +0x000 the physics descriptor (a u32 window address) */
    unsigned int tune;                  /* +0x004 its tune record (a u32 window address) */
    unsigned char body;                 /* +0x008 the car's body, 0-12 (the roster's car) */
    signed char input_flags;            /* +0x009 the human's input flags (its input record's) */
    signed char autotrans;              /* +0x00A automatic transmission */
    unsigned char pad00B[0x00F - 0x00B];
    signed char b00F;                   /* +0x00F a per-car table value (world_physics_tick: D_8011157C) */
    unsigned char pad010[0x220 - 0x010];
    float rwv[3];                       /* +0x220 the velocity, feet a second (MAT3 RWV) */
    float rwr[3];                       /* +0x22C the position, feet (MAT3 RWR) */
    unsigned char pad238[0x3F0 - 0x238];
    float speed;                        /* +0x3F0 the speed (compared with 5; the stunt end's) */
    unsigned char pad3F4[0x400 - 0x3F4];
    float scale;                        /* +0x400 */
    unsigned char pad404[0x544 - 0x404];
    float f544;                         /* +0x544 */
    unsigned char pad548[0x61C - 0x548];
    unsigned short viscode[4];          /* +0x61C each tire's visual code */
    unsigned char pad624[0x640 - 0x624];
    signed char crashflag;              /* +0x640 crashed: not drawn, not collidable */
    signed char topscrape;              /* +0x641 */
    unsigned char pad642[0x660 - 0x642];
    float resurrect_pos[3];             /* +0x660 the resurrect's position (tResurrect at +0x660) */
    unsigned char pad66C[0x6C4 - 0x66C];
    short moving_state;                 /* +0x6C4 the resurrect's state (negative: resurrecting) */
    unsigned char pad6C6[0x710 - 0x6C6];
    int iteration;                      /* +0x710 tFastIn: modelIteration */
    float model_time;                   /* +0x714 modelTime */
    float dt;                           /* +0x718 dt */
    short modelrun;                     /* +0x71C modelrun */
    unsigned char pad71E[0x720 - 0x71E];
    float steer;                        /* +0x720 the controls: steer -1..1 */
    unsigned char pad724[0x728 - 0x724];
    float brake;                        /* +0x728 brake 0..1 */
    float gas;                          /* +0x72C gas 0..1 */
    signed char gear;                   /* +0x730 */
    signed char flag_b;                 /* +0x731 */
    signed char flag_a;                 /* +0x732 */
    unsigned char pad733[0x75C - 0x733];
    float suspension[4];                /* +0x75C */
    unsigned char pad76C[0x794 - 0x76C];
    float reckon_rwr[3];                /* +0x794 the dead-reckoned position (tReckon at +0x734) */
    unsigned char pad7A0[0x7C4 - 0x7A0];
    short ob_res_pt;                    /* +0x7C4 obResPt: the path's high-water mark (path_D169C) */
    short slot;                         /* +0x7C6 the car slot (the roster's and the car record's index) */
    short active;                       /* +0x7C8 in the race */
    short we_control;                   /* +0x7CA this node drives it */
    signed char drone_type;             /* +0x7CC 1 a drone, 2 a human */
    unsigned char color[3];             /* +0x7CD the roster's three colours */
    short s7D0;                         /* +0x7D0 */
    unsigned char pad7D2[0x7D4 - 0x7D2];
    unsigned int flags;                 /* +0x7D4 appearance flags */
    float stuck_time;                   /* +0x7D8 */
    unsigned char pad7DC[0x7DE - 0x7DC];
    signed char can_cheat;              /* +0x7DE maxpath: the drone may cheat */
    signed char hide_car;               /* +0x7DF */
    unsigned char pad7E0[0x7E2 - 0x7E0];
    short last_cp;                      /* +0x7E2 last_cp_index */
    unsigned char pad7E4[0x7E6 - 0x7E4];
    short drone_target;                 /* +0x7E6 */
    signed char lap;                    /* +0x7E8 laps done */
    unsigned char pad7E9[0x7EA - 0x7E9];
    signed char collidable;             /* +0x7EA */
    unsigned char pad7EB[0x7EC - 0x7EB];
    float drone_scale;                  /* +0x7EC */
    float time_boost;                   /* +0x7F0 */
    unsigned char pad7F4[0x808 - 0x7F4];
};
extern struct pc_model pc_models[PC_MODEL_SLOTS];

/* The accessors: the array (PC_MODELS), record i's address (PC_MODEL) and the end of the N64's 6 (PC_MODELS_END:
 * menu_audio_settings walks the cars up to it, which on the N64 is the next symbol, D_8014D280), all void * so each
 * file's own view pointer takes them, and record i's fields. */
#define PC_MODELS ((void *)pc_models)
#define PC_MODEL(i) ((void *)&pc_models[i])
#define PC_MODELS_END ((void *)&pc_models[PC_N64_MODEL_SLOTS])
#define PC_MODEL_CAR(i) (pc_models[i].car)
#define PC_MODEL_BODY(i) (pc_models[i].body)
#define PC_MODEL_INPUT_FLAGS(i) (pc_models[i].input_flags)
#define PC_MODEL_AUTOTRANS(i) (pc_models[i].autotrans)
#define PC_MODEL_B00F(i) (pc_models[i].b00F)
#define PC_MODEL_RWV(i) (pc_models[i].rwv)
#define PC_MODEL_RWR(i) (pc_models[i].rwr)
#define PC_MODEL_SPEED(i) (pc_models[i].speed)
#define PC_MODEL_SCALE(i) (pc_models[i].scale)
#define PC_MODEL_F544(i) (pc_models[i].f544)
#define PC_MODEL_VISCODE(i) (pc_models[i].viscode)
#define PC_MODEL_CRASHFLAG(i) (pc_models[i].crashflag)
#define PC_MODEL_TOPSCRAPE(i) (pc_models[i].topscrape)
#define PC_MODEL_RESURRECT_POS(i) (pc_models[i].resurrect_pos)
#define PC_MODEL_MOVING_STATE(i) (pc_models[i].moving_state)
#define PC_MODEL_ITERATION(i) (pc_models[i].iteration)
#define PC_MODEL_MODEL_TIME(i) (pc_models[i].model_time)
#define PC_MODEL_DT(i) (pc_models[i].dt)
#define PC_MODEL_MODELRUN(i) (pc_models[i].modelrun)
#define PC_MODEL_STEER(i) (pc_models[i].steer)
#define PC_MODEL_BRAKE(i) (pc_models[i].brake)
#define PC_MODEL_GAS(i) (pc_models[i].gas)
#define PC_MODEL_GEAR(i) (pc_models[i].gear)
#define PC_MODEL_FLAG_B(i) (pc_models[i].flag_b)
#define PC_MODEL_FLAG_A(i) (pc_models[i].flag_a)
#define PC_MODEL_SUSPENSION(i) (pc_models[i].suspension)
#define PC_MODEL_RECKON_RWR(i) (pc_models[i].reckon_rwr)
#define PC_MODEL_OB_RES_PT(i) (pc_models[i].ob_res_pt)
#define PC_MODEL_SLOT(i) (pc_models[i].slot)
#define PC_MODEL_ACTIVE(i) (pc_models[i].active)
#define PC_MODEL_WE_CONTROL(i) (pc_models[i].we_control)
#define PC_MODEL_DRONE_TYPE(i) (pc_models[i].drone_type)
#define PC_MODEL_COLOR0(i) (pc_models[i].color[0])
#define PC_MODEL_COLOR1(i) (pc_models[i].color[1])
#define PC_MODEL_COLOR2(i) (pc_models[i].color[2])
#define PC_MODEL_S7D0(i) (pc_models[i].s7D0)
#define PC_MODEL_FLAGS(i) (pc_models[i].flags)
#define PC_MODEL_STUCK_TIME(i) (pc_models[i].stuck_time)
#define PC_MODEL_CAN_CHEAT(i) (pc_models[i].can_cheat)
#define PC_MODEL_HIDE_CAR(i) (pc_models[i].hide_car)
#define PC_MODEL_LAST_CP(i) (pc_models[i].last_cp)
#define PC_MODEL_DRONE_TARGET(i) (pc_models[i].drone_target)
#define PC_MODEL_LAP(i) (pc_models[i].lap)
#define PC_MODEL_COLLIDABLE(i) (pc_models[i].collidable)
#define PC_MODEL_DRONE_SCALE(i) (pc_models[i].drone_scale)
#define PC_MODEL_TIME_BOOST(i) (pc_models[i].time_boost)

#endif
