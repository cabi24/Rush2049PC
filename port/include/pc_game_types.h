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

/* ---- The car records: MAT3 gGameCar, tCarData, N64 player_array = D_80152818 (6 records of 0x3B8 bytes at
 * 0x80152818-0x80153E68, BSS; D_80153E68, a message queue, follows). One record per car slot: its position and
 * orientation for the cameras and effects, its visuals, its place and finish, and its battle state. On the PC a
 * record is 0x468 bytes (docs/port/CARS.md 2): its 21 visuals hold host pointers (0x20 bytes each, not 0x18) and
 * its input record pointer at N64 +0x380 is 8 bytes, so a field's PC offset is CAR_OFS of its N64 one
 * (pc_prelude.h); the padding below is in N64 bytes, between pointer-sized members, as in every view. Every walk
 * of it stops at 6 (the cars in the race, D_801543CA, D_80152744), so the storage is PC_CAR_SLOTS = the larger of
 * 6 and PC_MAX_LINKS; nothing activates more than 6 yet. Only the fields the game reaches by index (and the names
 * inside the array: D_80152907 is [0].finished) are named here; code that takes a record's address (PC_CAR) reads
 * it through its own view, which view_audit.py checks against the same layout. */
#define PC_N64_CAR_SLOTS 6
#define PC_CAR_SLOTS (PC_MAX_LINKS > PC_N64_CAR_SLOTS ? PC_MAX_LINKS : PC_N64_CAR_SLOTS)

/* A car visual (tVisual: N64 0x18 bytes, PC 0x20) on the PC, for the car record and the views that pad over its
 * 21 visuals at +0x110 (and the environment visual pool D_80138880): next and func are host pointers there, data
 * a u32 (docs/port/CARS.md 3). */
struct pc_visual {
    void *next;
    short index, objnum, slot;
    unsigned int data;
    float timeStamp;
    void (*func)(void *, short);
};

/* The drone's path state inside the car record (maxpath's Nav, func_800C4CF8's Region): no pointers, the same layout
 * on both. Code takes its address (&PC_CAR_NAV(i)) and reads it in its own view. */
struct pc_car_nav {
    float a, b, c, d;                   /* +0x00 */
    float spd;                          /* +0x10 the target speed */
    float tgt[3];                       /* +0x14 the target point */
    float tm;                           /* +0x20 the model time it was set */
    short pt;                           /* +0x24 the point on the path */
    short flag;                         /* +0x26 */
    short sel;                          /* +0x28 the path (Region's path) */
    short tk;                           /* +0x2A */
};

struct pc_car {                         /* the offsets are N64 ones; PC: CAR_OFS(n) */
    unsigned char pad000[0x008];
    float pos[3];                       /* +0x008 the position (MAT3 dr_pos), what the cameras and effects follow */
    unsigned char pad014[0x02C - 0x014];
    float uvs[3][3];                    /* +0x02C an orientation (menu_control_settings: the model's initial one) */
    float uvs50[3][3];                  /* +0x050 the orientation the cameras take (func_800EB90C) */
    float wheel_pos[4][3];              /* +0x074 each wheel's position (the skid marks) */
    unsigned char pad0A4[0x0E8 - 0x0A4];
    unsigned int flags;                 /* +0x0E8 appearance flags (0x8, 0x10, 0x1000, 0x100000) */
    unsigned char pad0EC[0x0EE - 0x0EC];
    signed char place;                  /* +0x0EE the race place, 0 the first */
    signed char finished;               /* +0x0EF 1 once it has finished: its place is locked */
    float time;                         /* +0x0F0 the finish time (car_setup_confirm: a score) */
    int node;                           /* +0x0F4 the scene node handle, -1 none (its low half: ENDIAN.md 3.1) */
    short mph;                          /* +0x0F8 the speed shown, mph x 4 */
    unsigned char pad0FA[0x100 - 0x0FA];
    float distance;                     /* +0x100 the distance along the track */
    unsigned char pad104[0x108 - 0x104];
    float f108;                         /* +0x108 zeroed at the race start (func_800FBE60) */
    unsigned char pad10C[0x110 - 0x10C];
    struct pc_visual visuals[21];       /* +0x110 the car's visuals (tVisual; the weapon's is [20]) */
    unsigned char pad308[0x314 - 0x308];
    struct pc_car_nav nav;              /* +0x314 the drone's path state (maxpath's Nav, Region) */
    float fwd_progress;                 /* +0x340 */
    unsigned char pad344[0x34C - 0x344];
    unsigned int ir_color;              /* +0x34C the IR colour (tRGBA's word) */
    unsigned int prev_ir_color;         /* +0x350 the one before */
    unsigned char pad354[0x356 - 0x354];
    short weight_index;                 /* +0x356 a drone's weight row (maxpath) */
    signed char b358;                   /* +0x358 cleared with the state (init_mdrive, the battle reset) */
    signed char state;                  /* +0x359 0 racing; below 2 still in the race; 1 out of a deaths race */
    signed char disabled;               /* +0x35A no speed from the wheels (func_800E23A4) */
    signed char slot;                   /* +0x35B its own slot, the index (the roster's, the model's) */
    signed char view;                   /* +0x35C the window showing it, -1 none (a bit of the windows' mask) */
    unsigned char pad35D[0x360 - 0x35D];
    int sheen_alpha;                    /* +0x360 */
    float heading;                      /* +0x364 */
    unsigned char pad368[0x380 - 0x368];
    void *input;                        /* +0x380 the input record &D_8014A118[i] (a host pointer), NULL for drones */
    signed char kind;                   /* +0x384 battle: the record's type (func_80393518 tests it for 8) */
    signed char lives;                  /* +0x385 battle: lives left, shown when not negative */
    short health;                       /* +0x386 battle: 800 full */
    unsigned char pad388[0x38C - 0x388];
    unsigned int battle_flags;          /* +0x38C battle: 1 and 2 pick the status shown */
    unsigned char pad390[0x3A0 - 0x390];
    unsigned char b3A0;                 /* +0x3A0 battle */
    unsigned char pad3A1[0x3A2 - 0x3A1];
    unsigned char b3A2;                 /* +0x3A2 battle */
    signed char points;                 /* +0x3A3 battle: points, or what is left (FindWinner ranks by it) */
    unsigned char pad3A4[0x3AC - 0x3A4];
    float reloading_time;               /* +0x3AC battle */
    float f3B0;                         /* +0x3B0 battle */
    float f3B4;                         /* +0x3B4 battle */
};
extern struct pc_car pc_cars[PC_CAR_SLOTS];

/* The accessors: the array (PC_CARS), record i's address (PC_CAR) and the end of the first n records
 * (PC_CARS_END: the walks over the cars in the race, D_80152818 + D_801543CA on the N64), all void * so each
 * file's own view pointer takes them, and record i's fields. PC_CAR_INPUT_AS(T, i) is the input pointer as a T *,
 * for code that reads through it in its own view. */
#define PC_CARS ((void *)pc_cars)
#define PC_CAR(i) ((void *)&pc_cars[i])
#define PC_CARS_END(n) ((void *)&pc_cars[n])
#define PC_CAR_POS(i) (pc_cars[i].pos)
#define PC_CAR_POS_X(i) (pc_cars[i].pos[0])
#define PC_CAR_POS_Y(i) (pc_cars[i].pos[1])
#define PC_CAR_POS_Z(i) (pc_cars[i].pos[2])
#define PC_CAR_UVS(i) (pc_cars[i].uvs)
#define PC_CAR_UVS50(i) (pc_cars[i].uvs50)
#define PC_CAR_WHEEL_POS(i) (pc_cars[i].wheel_pos)
#define PC_CAR_FLAGS(i) (pc_cars[i].flags)
#define PC_CAR_PLACE(i) (pc_cars[i].place)
#define PC_CAR_FINISHED(i) (pc_cars[i].finished)
#define PC_CAR_TIME(i) (pc_cars[i].time)
#define PC_CAR_NODE(i) (pc_cars[i].node)
#define PC_CAR_MPH(i) (pc_cars[i].mph)
#define PC_CAR_DISTANCE(i) (pc_cars[i].distance)
#define PC_CAR_F108(i) (pc_cars[i].f108)
#define PC_CAR_VISUALS(i) (pc_cars[i].visuals)
#define PC_CAR_NAV(i) (pc_cars[i].nav)
#define PC_CAR_FWD_PROGRESS(i) (pc_cars[i].fwd_progress)
#define PC_CAR_IR_COLOR(i) (pc_cars[i].ir_color)
#define PC_CAR_PREV_IR_COLOR(i) (pc_cars[i].prev_ir_color)
#define PC_CAR_WEIGHT_INDEX(i) (pc_cars[i].weight_index)
#define PC_CAR_B358(i) (pc_cars[i].b358)
#define PC_CAR_STATE(i) (pc_cars[i].state)
#define PC_CAR_DISABLED(i) (pc_cars[i].disabled)
#define PC_CAR_SLOT(i) (pc_cars[i].slot)
#define PC_CAR_VIEW(i) (pc_cars[i].view)
#define PC_CAR_SHEEN_ALPHA(i) (pc_cars[i].sheen_alpha)
#define PC_CAR_HEADING(i) (pc_cars[i].heading)
#define PC_CAR_INPUT(i) (pc_cars[i].input)
#define PC_CAR_INPUT_AS(T, i) ((T *)pc_cars[i].input)
#define PC_CAR_KIND(i) (pc_cars[i].kind)
#define PC_CAR_LIVES(i) (pc_cars[i].lives)
#define PC_CAR_HEALTH(i) (pc_cars[i].health)
#define PC_CAR_BATTLE_FLAGS(i) (pc_cars[i].battle_flags)
#define PC_CAR_B3A0(i) (pc_cars[i].b3A0)
#define PC_CAR_B3A2(i) (pc_cars[i].b3A2)
#define PC_CAR_POINTS(i) (pc_cars[i].points)
#define PC_CAR_RELOADING_TIME(i) (pc_cars[i].reloading_time)
#define PC_CAR_F3B0(i) (pc_cars[i].f3B0)
#define PC_CAR_F3B4(i) (pc_cars[i].f3B4)

#endif
