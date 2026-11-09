/* port/include/pc_game_types.h: game data the PC owns outright, with no N64 address (docs/port/DECOUPLE.md).
 * pc_prelude.h includes it, so every game C file sees it. Each item has one declaration here, storage gen_data
 * defines in data.c (port/data/decoupled.txt: outside the RDRAM window, zero at startup, or filled from the image if
 * the N64's was initialized data), and accessors the game C reaches it through. No system headers, as for
 * pc_prelude.h. */
#ifndef PC_GAME_TYPES_H
#define PC_GAME_TYPES_H

/* ---- The race roster: arcade and MAT3 gLink (13 records of 8 bytes on the N64, BSS).
 * One record per car slot: who drives it and how it starts. The N64 game activates 6 slots (its MAX_LINKS: the
 * per-car arrays are 6 long), but LoadCars, InitVisuals and InitSelectCarLoad walk all 13, so the storage is never
 * shorter than that. PC_MAX_LINKS is the most cars a race may hold on the PC (12, twice the N64's 6); nothing
 * activates more than 6 yet. */
#ifndef PC_MAX_LINKS
#define PC_MAX_LINKS 12
#endif
#define PC_N64_LINK_SLOTS 13
#define PC_LINK_SLOTS (PC_MAX_LINKS > PC_N64_LINK_SLOTS ? PC_MAX_LINKS : PC_N64_LINK_SLOTS)

/* The cars a race holds on the PC (docs/port/MORECARS.md 10.1): PC_N64_CARS, the N64's 6, unless the setting asks for
 * more, up to PC_MAX_LINKS. Game C reads it as PC_RACE_CARS where the N64 has a literal 6 for the race's cars; the
 * port defines pc_race_cars() (port/config/race_cars.c; the asset tests' in port/assets/tests/support.c), so game
 * files carry no ini code. */
#define PC_N64_CARS 6
int pc_race_cars(void);
void pc_race_cars_begin(void);
int pc_cars_setting(void);
#define PC_RACE_CARS (pc_race_cars())

/* [game] cars (port/config/race_cars.c, docs/port/FRONTEND.md). pc_race_cars() is the count latched by the last
 * pc_race_cars_begin(), which the game calls where a race's roster is filled (CalcPolePositions, the attract demo's
 * fake_init: MORECARS.md 10.2 item 3), so it never changes during a race, whatever the settings overlay does; it
 * is PC_N64_CARS until the first call. pc_cars_setting() is the setting itself, unlatched: PC_N64_CARS for original,
 * else 6 to PC_MAX_LINKS; the front end reads it (the track select's Drones option goes up to it less one). */

/* ---- Draw distance and detail (video.draw_distance, video.detail; docs/port/DETAIL.md; port/config/detail.c).
 * The game's scene walk reads the settings through these, every view of every frame.
 *   PC_DRAW_REACH(d)     a distance limit d (a model's last level of detail, the fog's radius gViewableDistance, the far
 *                        scenery's 1,600 ft): d at original, PC_DRAW_FAR_FACTOR times it at far, none (1e30) at max
 *   PC_LOD_DIST(d)       the distance the level of detail is chosen by: -1 (the nearest level) with detail high
 *   PC_DETAIL_PLAYERS(n) the player count a model's display lists keep their level-of-detail sections for (its LOD
 *                        markers, SetupWaveModel): 1 with detail high, the most a model has
 *   pc_draw_range_scale  the power of two a node's matrix is divided by to hold a place past 2,048 ft (0: left out)
 *   PC_SCENE(k)          a count of the scene's (pc_scene_count; the exit's scene line) */
#include "pc_detail.h"
int pc_draw_distance(void);   /* PC_DRAW_* (pc_detail.h) */
int pc_detail(void);          /* PC_DETAIL_* */
int pc_fog(void);             /* PC_FOG_*: off keeps the sky dome in a fog option (SkyInit, SkyInit.c) */
float pc_draw_reach(float d);
float pc_lod_distance(float d);
int pc_detail_players(int players);
int pc_draw_range_scale(float x, float y, float z);
enum { PC_SCENE_DRAWN, PC_SCENE_COARSE, PC_SCENE_PARTS, PC_SCENE_WORLD, PC_SCENE_HIDDEN, PC_SCENE_VIEW_HIDDEN,
       PC_SCENE_RANGE, PC_SCENE_RADIUS, PC_SCENE_LOD, PC_SCENE_COUNTS };
void pc_scene_view(void);                 /* a view's scene walk begins (zoid_draw_window) */
void pc_scene_count(int what, int n);
#define PC_DRAW_REACH(d) pc_draw_reach(d)
#define PC_LOD_DIST(d) pc_lod_distance(d)
#define PC_DETAIL_PLAYERS(n) pc_detail_players(n)
#define PC_SCENE(k) pc_scene_count(PC_SCENE_##k, 1);
#define PC_SCENE_LEVEL(lod) pc_scene_count(PC_SCENE_COARSE, (lod) != 0);   /* drawn at a level past its first */

/* ---- The pools a race fills, and the start grid (docs/port/MORECARS.md 8 and 4).
 * GetMatrix (ZOID_NewMatrix) hands out the persistent matrices, 48 bytes each (a scene node's, a blast's): the N64's
 * PC_N64_MATRICES at gMatrices (index 0 unused), then PC_MATRIX_SPILL more in
 * pc_matrix_spill, a range of the RDRAM window the PC reserves (port/data/reserved.txt: bytes the decoupled model
 * records freed). They stay in the window: a window's camera matrix is kept as a u32 (skyMatrices, MORECARS.md 2.7).
 * About 12 a car: a 6-car race on track 0 takes indexes 1-130, a 12-car one 1-202. */
#define PC_N64_MATRICES 150
#ifndef PC_MATRIX_SPILL
#define PC_MATRIX_SPILL 128
#endif
#define PC_MATRICES (PC_N64_MATRICES + PC_MATRIX_SPILL)
extern float pc_matrix_spill[PC_MATRIX_SPILL][12];
/* The scene nodes (ZOID_AddObject: ObjInstanceList, own storage), the N64's 600 and checked: 12-13 a car on the retail
 * tracks, where a 12-car race takes up to 526 (two views; a 6-car one 454). */
#define PC_SCENE_NODES 600
/* The frame's command buffers, two that alternate (pc_frame_dl, initgraphics/initgfx.c; StartGraphics.c): 1,728
 * bytes of viewports, vertices, projections, lookats and the identity (particle_knot's GfxBuf), PC_FRAME_MATRICES
 * matrices of 64 bytes (frame_matrix_cursor, from +1,728), then the display list (frame_dl from PC_GLIST_OFFSET),
 * PC_GLIST_LEN commands of 16 bytes. The N64's (frame_command_buffer_a, frame_command_buffer_b) hold 600 and 4,000: the list at +0x9CC0.
 * video.draw_distance max with two views and 12 cars takes up to 870 matrices and 6,347 commands on the retail tracks
 * (docs/port/DETAIL.md 4), so 1,600 and 12,000 (1,200 and 8,000 were MORECARS.md 8.1's, for 546 and 4,131). */
#ifndef PC_FRAME_MATRICES
#define PC_FRAME_MATRICES 1600
#endif
#ifndef PC_GLIST_LEN
#define PC_GLIST_LEN 12000
#endif
#define PC_FRAME_MATRIX_OFFSET 1728
#define PC_FRAME_LOOKATS 32   /* the GfxBuf's la[32]: one per environment-mapped node a view draws */
#define PC_GLIST_OFFSET (PC_FRAME_MATRIX_OFFSET + PC_FRAME_MATRICES * 64)
#define PC_FRAME_BYTES (PC_GLIST_OFFSET + PC_GLIST_LEN * 16)
/* A pool run past its end (port/ultra/pools.c): it says so, once a pool, and a Debug build stops there.
 * pc_frame_pools checks the frame's two at the end of its list (EndGraphics: the frame, the list's end
 * and the matrices' end) and keeps their high-water marks; it returns 1 if either ran past its end. */
void pc_pool_overflow(const char *pool, int used, int capacity);
int pc_frame_pools(const void *frame, const void *glist_end, const void *matrices_end);
/* The start grid. Resurrect offsets a car from the start point by its place's row of pole_pos_offset (MAT3
 * resurrect.c's, f32[12][3], game data): rows 0-5 the N64's grid, rows 6-9 the humans' respawns. On the N64 a respawn
 * is coded as a place of 6 or more (index + 6, 6 in a ghost race), so places 6-11 have no grid row. On the PC a respawn
 * is a place of PC_RESPAWN_BASE or more and reads the ROM's respawn rows as before (place - PC_RESPAWN_BASE + 6), and
 * pc_pole_row gives every start place a row: 0-5 the loaded table's, 6 on the two columns continued backwards at the
 * loaded rows' spacing; a place from 6 on starts from the resurrection path's point its row's distance behind the start
 * point, so the rows follow the road (groups/resurrect/resurrect.c: pc_grid_start). */
#define PC_RESPAWN_BASE PC_MAX_LINKS
float *pc_pole_row(int place);

struct pc_link {
    unsigned char track;     /* +0 the track (CalcPolePositions, fake_init: gTrackNum) */
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

/* ---- The model records: MAT3 model[], tModelData (6 records of 0x808 bytes on the N64, BSS). One record per car slot:
 * its physics state, controls and race state. The record keeps its N64 layout on the PC (its pointer fields are u32
 * window addresses, docs/port/CARS.md 4), so its size is 0x808 here too and code that steps through it by 0x808 still
 * lands on the fields. Every walk of it stops at 6 (the cars in the race, gNumCars, gNumActiveCars, MAX_LINKS), so the
 * storage is PC_MODEL_SLOTS = the larger of 6 and PC_MAX_LINKS; nothing activates more than 6 yet. Only the fields the
 * game reaches by index (and the labels the image's symbol tables have inside the array, record 0's slot ...) are named
 * here; code that takes a record's address (PC_MODEL) reads it through its own view, which view_audit.py checks against
 * the same layout. */
#define PC_N64_MODEL_SLOTS 6
#define PC_MODEL_SLOTS (PC_MAX_LINKS > PC_N64_MODEL_SLOTS ? PC_MAX_LINKS : PC_N64_MODEL_SLOTS)

struct pc_model {
    unsigned int car;                   /* +0x000 the physics descriptor (a u32 window address) */
    unsigned int tune;                  /* +0x004 its tune record (a u32 window address) */
    unsigned char body;                 /* +0x008 the car's body, 0-12 (the roster's car) */
    signed char input_flags;            /* +0x009 the human's input flags (its input record's) */
    signed char autotrans;              /* +0x00A automatic transmission */
    unsigned char pad00B[0x00F - 0x00B];
    signed char tire_rims;              /* +0x00F the rims (MAT3 tireRims; init_cars: gTireRims) */
    unsigned char pad010[0x220 - 0x010];
    float rwv[3];                       /* +0x220 the velocity, feet a second (MAT3 RWV) */
    float rwr[3];                       /* +0x22C the position, feet (MAT3 RWR) */
    unsigned char pad238[0x3F0 - 0x238];
    float speed;                        /* +0x3F0 the speed (compared with 5; the stunt end's) */
    unsigned char pad3F4[0x400 - 0x3F4];
    float catchup;                      /* +0x400 the catch-up scale of the car's time step (MAT3 catchup) */
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
    signed char wing;                   /* +0x731 the wing button (MAT3 mainin.wing) */
    signed char abort;                  /* +0x732 the abort button (MAT3 mainin.abort) */
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
    short rpm;                          /* +0x7D0 the engine's rpm shown (MAT3 rpm; animTach) */
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

/* The accessors: the array (PC_MODELS), record i's address (PC_MODEL) and the end of the race's cars (PC_MODELS_END,
 * the first PC_RACE_CARS records: collision walks the cars up to it, which on the N64 is the next symbol,
 * gTargetArray, the end of its 6), all void * so each file's own view pointer takes them, and record i's fields. */
#define PC_MODELS ((void *)pc_models)
#define PC_MODEL(i) ((void *)&pc_models[i])
#define PC_MODELS_END ((void *)&pc_models[PC_RACE_CARS])
#define PC_MODEL_CAR(i) (pc_models[i].car)
#define PC_MODEL_BODY(i) (pc_models[i].body)
#define PC_MODEL_INPUT_FLAGS(i) (pc_models[i].input_flags)
#define PC_MODEL_AUTOTRANS(i) (pc_models[i].autotrans)
#define PC_MODEL_B00F(i) (pc_models[i].tire_rims)
#define PC_MODEL_RWV(i) (pc_models[i].rwv)
#define PC_MODEL_RWR(i) (pc_models[i].rwr)
#define PC_MODEL_SPEED(i) (pc_models[i].speed)
#define PC_MODEL_SCALE(i) (pc_models[i].catchup)
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
#define PC_MODEL_FLAG_B(i) (pc_models[i].wing)
#define PC_MODEL_FLAG_A(i) (pc_models[i].abort)
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
#define PC_MODEL_S7D0(i) (pc_models[i].rpm)
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

/* ---- The car records: MAT3 gGameCar, tCarData, N64 gGameCar (6 records of 0x3B8 bytes, BSS; sample_dma_pi_queue, a
 * message queue, follows). One record per car slot: its position and orientation for the cameras and effects, its
 * visuals, its place and finish, and its battle state. On the PC a record is 0x468 bytes (docs/port/CARS.md 2): its 21
 * visuals hold host pointers (0x20 bytes each, not 0x18) and its input record pointer at N64 +0x380 is 8 bytes, so a
 * field's PC offset is CAR_OFS of its N64 one (pc_prelude.h); the padding below is in N64 bytes, between pointer-sized
 * members, as in every view. Every walk of it stops at 6 (the cars in the race, gNumCars, gNumActiveCars), so the
 * storage is PC_CAR_SLOTS = the larger of 6 and PC_MAX_LINKS; nothing activates more than 6 yet. Only the fields the
 * game reaches by index (and the labels the image's symbol tables have inside the array, record 0's finished ...) are
 * named here; code that takes a record's address (PC_CAR) reads it through its own view, which view_audit.py checks
 * against the same layout. */
#define PC_N64_CAR_SLOTS 6
#define PC_CAR_SLOTS (PC_MAX_LINKS > PC_N64_CAR_SLOTS ? PC_MAX_LINKS : PC_N64_CAR_SLOTS)

/* A car visual (tVisual: N64 0x18 bytes, PC 0x20) on the PC, for the car record and the views that pad over its
 * 21 visuals at +0x110 (and the environment visual pool gEnvirons): next and func are host pointers there, data
 * a u32 (docs/port/CARS.md 3). */
struct pc_visual {
    void *next;
    short index, objnum, slot;
    unsigned int data;
    float timeStamp;
    void (*func)(void *, short);
};

/* The drone's path state inside the car record (MAT3 tCarData.mpctl, a tPathPoint, with its field names; maxpath.c):
 * no pointers, the same layout on both. Code takes its address (&PC_CAR_NAV(i)). */
struct pc_car_nav {
    float xrel;                         /* +0x00 how far along the current stretch of drone path the car is */
    float yrel;                         /* +0x04 how far to the side of it */
    float cyrel;                        /* +0x08 the side offset of the point after next, from the stretch */
    float len;                          /* +0x0C the stretch's length */
    float tgtspd;                       /* +0x10 the target speed, feet a second */
    float tgtpos[3];                    /* +0x14 the target point, then the steering direction */
    float interval_time;                /* +0x20 the model time the car reached this stretch */
    short idx;                          /* +0x24 the stretch's first point on the path */
    short newIdx;                       /* +0x26 a point to jump to, -1 none */
    short path;                         /* +0x28 the drone path followed (gDronePaths) */
    short basePath;                     /* +0x2A the drone path the car started on, which it prefers */
};

struct pc_car {                         /* the offsets are N64 ones; PC: CAR_OFS(n) */
    unsigned char pad000[0x008];
    float pos[3];                       /* +0x008 the position (MAT3 dr_pos), what the cameras and effects follow */
    unsigned char pad014[0x02C - 0x014];
    float uvs[3][3];                    /* +0x02C an orientation (init_road: the model's initial one) */
    float uvs50[3][3];                  /* +0x050 the orientation the cameras take (CheckCameraView) */
    float wheel_pos[4][3];              /* +0x074 each wheel's position (the skid marks) */
    unsigned char pad0A4[0x0E8 - 0x0A4];
    unsigned int flags;                 /* +0x0E8 appearance flags (0x8, 0x10, 0x1000, 0x100000) */
    unsigned char pad0EC[0x0EE - 0x0EC];
    signed char place;                  /* +0x0EE the race place, 0 the first */
    signed char place_locked;           /* +0x0EF 1 once it has finished: its place is locked (MAT3 place_locked) */
    float score;                        /* +0x0F0 the finish time or score CarReportsGameOver ranks by (MAT3 score) */
    int node;                           /* +0x0F4 the scene node handle, -1 none (its low half: ENDIAN.md 3.1) */
    short mph;                          /* +0x0F8 the speed shown, mph x 4 */
    unsigned char pad0FA[0x100 - 0x0FA];
    float distance;                     /* +0x100 the distance along the track */
    unsigned char pad104[0x108 - 0x104];
    float mileage;                      /* +0x108 the distance driven (MAT3 mileage; InitCPTime zeroes it) */
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
    signed char crash_state;            /* +0x358 MAT3 crash_state (init_mdrive and the battle reset clear it) */
    signed char dead;                   /* +0x359 MAT3 dead: 0 racing, below 2 in the race, 1 out of a deaths race */
    signed char in_tunnel;              /* +0x35A MAT3 in_tunnel: forces1 turns the wind off, the sheen fades */
    signed char slot;                   /* +0x35B its own slot, the index (the roster's, the model's) */
    signed char camera;                 /* +0x35C the window showing it, -1 none (MAT3 camera; a bit of the mask) */
    unsigned char pad35D[0x360 - 0x35D];
    int sheen_alpha;                    /* +0x360 */
    float stray_angle;                  /* +0x364 its angle to the way round (MAT3 strayAngle: the WRONG WAY sign) */
    unsigned char pad368[0x380 - 0x368];
    void *input;                        /* +0x380 the input record &gPlayers[i] (a host pointer), NULL for drones */
    signed char weapon_type;            /* +0x384 battle: the weapon (MAT3 battle.weapon.type; WEAPON_NONE 8) */
    signed char ammo_count;             /* +0x385 battle: its ammunition, shown when not negative (weapon.ammoCnt) */
    short health;                       /* +0x386 battle: 800 full */
    unsigned char pad388[0x38C - 0x388];
    unsigned int defense_active;        /* +0x38C battle: the defence on (MAT3 defense.active; 1 and 2 pick the icon) */
    unsigned char pad390[0x3A0 - 0x390];
    unsigned char shoot;                /* +0x3A0 battle: MAT3 battle.shoot */
    unsigned char pad3A1[0x3A2 - 0x3A1];
    unsigned char fade_state;           /* +0x3A2 battle: MAT3 battle.fadeState */
    signed char points;                 /* +0x3A3 battle: points, or what is left (FindWinner ranks by it) */
    unsigned char pad3A4[0x3AC - 0x3A4];
    float reloading_time;               /* +0x3AC battle */
    float battle_pitch;                 /* +0x3B0 battle: MAT3 battle.pitch */
    float battle_yaw;                   /* +0x3B4 battle: MAT3 battle.yaw */
    unsigned int scenes;                /* PC +0x464, in the record's alignment padding, no N64 field: car 8 and up,
                                         * bit n set while it is in a scene trigger of collision index n (0-31),
                                         * which the trigger's flags have no bit for (TargetCollTrigger) */
};
extern struct pc_car pc_cars[PC_CAR_SLOTS];

/* The accessors: the array (PC_CARS), record i's address (PC_CAR) and the end of the first n records
 * (PC_CARS_END: the walks over the cars in the race, gGameCar + gNumCars on the N64), all void * so each
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
#define PC_CAR_FINISHED(i) (pc_cars[i].place_locked)
#define PC_CAR_TIME(i) (pc_cars[i].score)
#define PC_CAR_NODE(i) (pc_cars[i].node)
#define PC_CAR_MPH(i) (pc_cars[i].mph)
#define PC_CAR_DISTANCE(i) (pc_cars[i].distance)
#define PC_CAR_F108(i) (pc_cars[i].mileage)
#define PC_CAR_VISUALS(i) (pc_cars[i].visuals)
#define PC_CAR_NAV(i) (pc_cars[i].nav)
#define PC_CAR_FWD_PROGRESS(i) (pc_cars[i].fwd_progress)
#define PC_CAR_IR_COLOR(i) (pc_cars[i].ir_color)
#define PC_CAR_PREV_IR_COLOR(i) (pc_cars[i].prev_ir_color)
#define PC_CAR_WEIGHT_INDEX(i) (pc_cars[i].weight_index)
#define PC_CAR_B358(i) (pc_cars[i].crash_state)
#define PC_CAR_STATE(i) (pc_cars[i].dead)
#define PC_CAR_DISABLED(i) (pc_cars[i].in_tunnel)
#define PC_CAR_SLOT(i) (pc_cars[i].slot)
#define PC_CAR_VIEW(i) (pc_cars[i].camera)
#define PC_CAR_SHEEN_ALPHA(i) (pc_cars[i].sheen_alpha)
#define PC_CAR_HEADING(i) (pc_cars[i].stray_angle)
#define PC_CAR_INPUT(i) (pc_cars[i].input)
#define PC_CAR_INPUT_AS(T, i) ((T *)pc_cars[i].input)
#define PC_CAR_KIND(i) (pc_cars[i].weapon_type)
#define PC_CAR_LIVES(i) (pc_cars[i].ammo_count)
#define PC_CAR_HEALTH(i) (pc_cars[i].health)
#define PC_CAR_BATTLE_FLAGS(i) (pc_cars[i].defense_active)
#define PC_CAR_B3A0(i) (pc_cars[i].shoot)
#define PC_CAR_B3A2(i) (pc_cars[i].fade_state)
#define PC_CAR_POINTS(i) (pc_cars[i].points)
#define PC_CAR_RELOADING_TIME(i) (pc_cars[i].reloading_time)
#define PC_CAR_SCENES(i) (pc_cars[i].scenes)
#define PC_CAR_F3B0(i) (pc_cars[i].battle_pitch)
#define PC_CAR_F3B4(i) (pc_cars[i].battle_yaw)

/* ---- The effects' per-car arrays (docs/port/DECOUPLE.md 6, MORECARS.md 2.3): the tire effects, the blasts, the skid
 * marks and the resurrect's path, one entry per car slot, 6 on the N64 (game BSS), PC_MAX_LINKS here. The resets
 * clear every slot (InitSmoke, InitBlasts, InitSkids: PC_MAX_LINKS on the PC), and the rest reaches
 * one car's entry by its slot. No pointer to them is kept as a 32-bit N64 address (the scene nodes that point at the
 * blasts' matrices hold host pointers), so they need no window address. On the PC a reader's N64 name is the PC
 * array where its view has the PC declaration's field names, and otherwise the array seen as PC_MAX_LINKS of its own
 * element type (#define gBlastRingObjs (*(Effect (*)[PC_MAX_LINKS]) PC_BLAST_RINGS)), so the uses keep their N64 tokens
 * and -fsanitize=bounds checks each index against PC_MAX_LINKS. */
/* N64 gCarSmokeDir (u32[6]): each car's tire-effect latches, a bit per tire and effect (StartSmoke,
 * StartWater, UpdateSmoke, InitWingSmoke, InitSmokeBlast). */
extern unsigned int pc_tire_fx[PC_MAX_LINKS];
extern unsigned char pc_smoke_shade[PC_MAX_LINKS];  /* N64 gCarBlastSmokeClr (u8[6], 8 by the gap): the grey of its smoke */
extern float pc_trail_pts[PC_MAX_LINKS][6];         /* N64 gCarWingSmokePos: its two trails' last points */
extern float pc_resurrect_mid[PC_MAX_LINKS][3];     /* N64 resurrect_save_pos: the resurrect's midpoint */

/* MAT3 tUVMatrix, tBlastObj, tBlastStruct and tBlastRingStruct (GameSource visuals.c), with MAT3's field names, which
 * blast.c's and AnimateNewBlast's views have too: no pointers, the same layout on both. */
struct pc_uv_matrix { float uvs[3][3]; float pos[3]; };
struct pc_blast_obj {                   /* 0x54 */
    int mbIdx;                          /* +0x00 its scene object, -1 none */
    struct pc_uv_matrix mat;            /* +0x04 */
    float scale, scaleInc, yaw, yawInc, pitch, pitchInc;
    unsigned char alpha, alphaInc;      /* +0x4C */
    short unused;
    float ht;                           /* +0x50 */
};
struct pc_blast {                       /* 0x190: N64 gBlastObjs, 6 of them */
    struct pc_blast_obj blastObjs[4];   /* +0x000 */
    int glowIdx;                        /* +0x150 the glow's scene object, -1 none */
    struct pc_uv_matrix mat;            /* +0x154 the glow's matrix: its scene node points here */
    unsigned char alpha, alphaCnt, ralpha, ralphaCnt;
    float lifeTime, incTime;            /* +0x188 */
};
struct pc_blast_ring {                  /* 0x3C: N64 gBlastRingObjs, 6 of them to gTempCircuitFirstTrack */
    int ringIdx;                        /* +0x00 its scene object, -1 none (AnimateNewBlastRing: its low half) */
    struct pc_uv_matrix mat;            /* +0x04 its scene node points here */
    unsigned char ralpha, ralphaCnt;    /* +0x34 */
    short unused;
    float incTime;                      /* +0x38 */
};
/* The arcade NewSkid (skid_marks' WheelSlot, with its field names): its segment pointer is a host pointer (the skid
 * segment pool's, own storage), so it is 0x60 bytes here and 0x5C on the N64. */
struct pc_skid {
    void *obj;                          /* +0x00 the skid segment drawn, NULL none */
    float start[3], pos[3], lenSq, dir[3], quad[6], left[3], right[3];
};
extern struct pc_blast pc_blasts[PC_MAX_LINKS];
extern struct pc_blast_ring pc_blast_rings[PC_MAX_LINKS];
extern struct pc_skid pc_skids[PC_MAX_LINKS][4];    /* N64 gNewSkid: each car's four tires */

/* The record arrays as void *, for the readers' own views. */
#define PC_BLASTS ((void *)pc_blasts)
#define PC_BLAST_RINGS ((void *)pc_blast_rings)
#define PC_SKIDS ((void *)pc_skids)

/* ---- The cars' sounds, checkpoints and finish cameras (docs/port/MORECARS.md 2.3, docs/port/DECOUPLE.md 7). Per car
 * slot, 6 on the N64, PC_MAX_LINKS here; the N64 symbols are retired. A sound record is a sound manager handle and the
 * values last sent to it (InitSoundState sets -1 and -2.0f: none); each file reads it through its own view
 * (State20, SndRec, LayerState: 20 bytes on both), so the record accessors are void *.
 *   pc_engine_sounds (D_80140420, 6 x 84 B, BSS; own storage before, the table pointer making it 88 B here):
 *     StartCarSounds (d5e64.c) sets the car type's engine table and starts layers 0 and 1, the engine update
 *     (UpdateEngineSound, mode.c) sets their pitch and volume and keeps a random sound in record 3, StopCarSounds
 *     (StopCarSounds) releases 0, 1 and 3. Record 2 is never used.
 *   pc_screech_sounds (D_80140640, 6 x 20 B, BSS): the tire screech, reset per car, played for humans (mode.c).
 *   pc_car_sounds_on (D_8010FFC4, 6 B, data, zero in the image): StartCarSounds sets it, StopCarSounds clears it.
 *   pc_engine_phase (D_8010FFCC, 6 B, data): each car's engine is updated every other frame, the cars alternating
 *     (UpdateCarSounds skips the frames it is 1, toggling it; StopCarSounds sets it to (slot & 1) == 0). The image's
 *     alternation is filled in at startup, and slots 6 on continue it with a period of 2 (decoupled.txt).
 *   pc_cp_last_pos (D_80152218, 6 x 12 B, BSS): each car's position at the last CheckCPs, for the distance it
 *     drove (car_checkpoints_CheckCPs.c), set at the race's start by InitCPTime.
 *   pc_cp_plane (D_801527E8, 6 x 4 B, BSS): each car's distance to its next checkpoint's plane at the last CheckCPs.
 *   pc_finish_cam, pc_finish_cam_time (D_80110680, 6 B, and D_80110668, 6 x 4 B, data, zero in the image): a human's
 *     circling camera after the finish (PassedCP sets 1 and the time, circle_camera_around_car freezes it at 2
 *     after 10 s, CheckCameraView turns the view to it, init_views and InitCheckpointData clear it). */
struct pc_sound_rec {
    int handle;                 /* +0x0 the sound manager's handle, -1 none */
    float level;                /* +0x4 the last level sent (AudChangeEffectVol: the engine's pitch) */
    float style;                /* +0x8 the last style sent (AudChangeEffectPitch: the volume) */
    float f0C, f10;             /* +0xC, +0x10 */
};
struct pc_engine_sound {
    void *table;                /* +0x0 the car type's engine table, &gEngineSpecs[type] (two 32-byte layers) */
    struct pc_sound_rec rec[4]; /* +0x4 (PC +0x8) layers 0 and 1, an unused one, the random sound */
};
extern struct pc_engine_sound pc_engine_sounds[PC_MAX_LINKS];
extern struct pc_sound_rec pc_screech_sounds[PC_MAX_LINKS];
extern signed char pc_car_sounds_on[PC_MAX_LINKS];
extern signed char pc_engine_phase[PC_MAX_LINKS];
extern float pc_cp_last_pos[PC_MAX_LINKS][3];
extern float pc_cp_plane[PC_MAX_LINKS];
extern unsigned char pc_finish_cam[PC_MAX_LINKS];
extern float pc_finish_cam_time[PC_MAX_LINKS];

/* The accessors: car i's records (void *, for each file's view) and fields. */
#define PC_ENGINE_SOUND(i) ((void *)&pc_engine_sounds[i])
#define PC_ENGINE_SOUND_REC(i, k) ((void *)&pc_engine_sounds[i].rec[k])
#define PC_ENGINE_SOUND_HANDLE(i, k) (pc_engine_sounds[i].rec[k].handle)
#define PC_SCREECH(i) ((void *)&pc_screech_sounds[i])
#define PC_SCREECH_LEVEL(i) (pc_screech_sounds[i].level)
#define PC_SCREECH_STYLE(i) (pc_screech_sounds[i].style)
#define PC_CAR_SOUNDS_ON(i) (pc_car_sounds_on[i])
#define PC_ENGINE_PHASE(i) (pc_engine_phase[i])
#define PC_CP_LAST_POS(i) (pc_cp_last_pos[i])
#define PC_CP_LAST_POS_X(i) (pc_cp_last_pos[i][0])
#define PC_CP_LAST_POS_Y(i) (pc_cp_last_pos[i][1])
#define PC_CP_LAST_POS_Z(i) (pc_cp_last_pos[i][2])
#define PC_CP_PLANE(i) (pc_cp_plane[i])
#define PC_FINISH_CAM(i) (pc_finish_cam[i])
#define PC_FINISH_CAM_TIME(i) (pc_finish_cam_time[i])

/* ---- The drones (docs/port/DECOUPLE.md 8): MAT3 drones[] and our_drones[] (s16 lists of car slots, 6 long on the N64,
 * BSS): the drones in the race (num_drones of them) and those this node drives (num_our_drones), built by munge
 * (munge_gLink_data) and PrePlay's restart, read by maxpath's camera pick (IntervalPosChange) and update_drone_models.
 * And maxpath's controls per car slot (arcade maxpath.c), BSS, 6 on the N64: the steer, brake and gas PS_PurgeFile and
 * AdjustSpeed keep (D_80153F28, D_80153F48, D_80153F68: f32), and avoid_areas' last direction and last direction to the
 * target (D_80153F88, D_80154138: f32[3]). The race's 12 cars may all be drones (the attract demo), so each is
 * PC_MAX_LINKS long. The N64's extents end at the next symbol, or at one of HAL_DrawText's statics (lastFontColor,
 * lastTrans, lastFontXpos, lastFontYpos), which are host statics on the PC. */
extern short pc_drones[PC_MAX_LINKS];
extern short pc_our_drones[PC_MAX_LINKS];
extern float pc_mp_steer[PC_MAX_LINKS];
extern float pc_mp_brake[PC_MAX_LINKS];
extern float pc_mp_gas[PC_MAX_LINKS];
extern float pc_mp_dir[PC_MAX_LINKS][3];
extern float pc_mp_save_dir[PC_MAX_LINKS][3];
#define PC_DRONE(j) (pc_drones[j])
#define PC_OUR_DRONE(j) (pc_our_drones[j])
#define PC_MP_STEER(slot) (pc_mp_steer[slot])
#define PC_MP_BRAKE(slot) (pc_mp_brake[slot])
#define PC_MP_GAS(slot) (pc_mp_gas[slot])
#define PC_MP_DIR(slot) (pc_mp_dir[slot])
#define PC_MP_SAVE_DIR(slot) (pc_mp_save_dir[slot])

/* CarSetup's flag per car slot (carsetup_flag, N64 carsetup_flag: s8, 6 cleared by PrePlay, BSS; the
 * controllers' gAutoEdges follows at +8): set once the car's visuals were started at the race start. */
extern signed char pc_carsetup_flags[PC_MAX_LINKS];
#define PC_CARSETUP_FLAG(slot) (pc_carsetup_flags[slot])

/* The drone schedule's rows (MAT3 drone_sched, ROM data, Row3[12]: rows 0-6 for 0-6 drones with one
 * human or none, rows 7-11 for 0-4 with two or more). init_cars picks row num_our_drones, plus 7 with two
 * humans; a human's retirement takes 7 off, a drone's 1. On the PC the second kind starts at PC_DRONE_SCHED_TWO,
 * past the most drones a race may hold, and update_drone_models reads the N64's rows where it has them and a PC
 * rule past them (update_model/group.c). */
#define PC_DRONE_SCHED_TWO (PC_MAX_LINKS + 1)


/* ---- MusyX's voices and the sound manager's pools: [audio] voices and [audio] engines (docs/port/AUDIO.md 10;
 * port/config/voices.c; the asset tests' in port/assets/tests/support.c). The N64 game gives MusyX 32 voices
 * (AudioInit's first argument, AudioInitSys' sound_lib_init caps it at 32): 16 for effects, 16 for the songs. On the
 * PC the run's voices are pc_mx_voices(), 32 at original, else 48, 64 or 96, latched at start, since the library
 * is set up once a run; the effects get all but the songs' 16. PC_MX_SLOTS is where the N64 has a literal 32 for the
 * voices (its fixed loops and caps): the run's count, never below 32, so at original every loop is the N64's.
 * pc_snd_setup gives AudioInit its arguments for the Expansion Pak's set-up (32 voices): the voices, the effects'
 * share, which is also the sound manager's budget base (gMaxSfx: it plays 3/4 of it at once) and AudioSfxInit's
 * entries, and the positional emitters; at original it changes nothing, and the set-up without the pak (8 voices)
 * is never changed. */
#define PC_MX_N64_VOICES 32
#define PC_MX_VOICE_SLOTS 96   /* the most pc_mx_voices() gives: the per-voice storage below is this long */
int pc_mx_voices(void);
#define PC_MX_SLOTS (pc_mx_voices())
/* In the library's C, where the N64 has a literal n (32) for the voices: PC_MX_LOOP(n) the run's voices (a loop over
 * them, a cap), PC_MX_CAP(n) the most a run can have (sound_lib_init's cap). The n stays, the N64's number. */
#define PC_MX_LOOP(n) PC_MX_SLOTS
#define PC_MX_CAP(n) PC_MX_VOICE_SLOTS
#define PC_SND_EMITTERS 128    /* the positional emitters above original (the N64's 64; AUDIO.md 10 says why) */
void pc_snd_setup(unsigned char *voices, unsigned char *sfx, int *emitters);

/* The library's per-voice storage, 32 entries in the boot segment's BSS on the N64 (docs/port/DECOUPLE.md 9), each
 * indexed by a voice's number (the low byte of its id): its state (0x1A0 bytes, the N64 layout: macros.c's and
 * the other units' views, MUSYX_DATA.md 8.3), its sequence node (voices.c's SequenceNode: two host pointers, the
 * key and the voice id, packed), its links in the priority lists and in the free list ({u8 previous, next; u16},
 * 255 for none), an effect voice's 134 MIDI controllers (an effect voice is its own channel in set 255,
 * controllers.c), its two per-channel bytes there (sound_midi_set_last_key, sound_midi_set_portamento_state) and its legato owner (macros.c's
 * sound_synth_claim_channel), and a stream's buffer record (streams_3d.c's SampleBuffer). Each reader's N64 name is the PC array
 * seen in its own view, PC_MX_VOICE_SLOTS long, so the uses keep their N64 tokens. */
extern unsigned char pc_mx_voice[PC_MX_VOICE_SLOTS][0x1A0];   /* N64 sound_synth_voices */
#pragma pack(1)
struct pc_mx_node { struct pc_mx_node *next, *previous; unsigned int key; int value; };
#pragma pack()
extern struct pc_mx_node pc_mx_nodes[PC_MX_VOICE_SLOTS];      /* N64 sound_handle_nodes */
struct pc_mx_link { unsigned char previous, next; unsigned short flags; };
extern struct pc_mx_link pc_mx_prio_links[PC_MX_VOICE_SLOTS]; /* N64 sound_voice_priority_links */
extern struct pc_mx_link pc_mx_free_links[PC_MX_VOICE_SLOTS]; /* N64 sound_voice_free_links */
extern unsigned char pc_mx_fx_ctrl[PC_MX_VOICE_SLOTS][134];   /* N64 sound_voice_controllers */
extern unsigned char pc_mx_fx_chan_a[PC_MX_VOICE_SLOTS];      /* N64 sound_voice_last_keys */
extern unsigned char pc_mx_fx_chan_b[PC_MX_VOICE_SLOTS];      /* N64 sound_voice_portamento */
extern unsigned char pc_mx_fx_legato[PC_MX_VOICE_SLOTS];      /* N64 sound_voice_channel_owners */
struct pc_mx_stream {   /* sound_sample_record.h's SampleBuffer */
    unsigned char mode, unknown01[3];
    int (*callback)(short *, unsigned int, short *, unsigned int, unsigned int);
    short *buffer;
    unsigned int samples, position, context;
};
extern struct pc_mx_stream pc_mx_streams[PC_MX_VOICE_SLOTS];  /* N64 sound_streams */

/* The sound manager's messages (N64 audiomgr_gCmds: 128 commands, 32 bytes each on the PC, own storage) and the two lists
 * of the commands a priority pass sends (gStartCmds gStartCmds, gStopCmds gStopCmds: 128 pointers each, own
 * storage), PC data of PC_SND_MSG_SLOTS; the game walks pc_snd_messages() of them, the N64's 128 at original and 256
 * with more voices, and the manager's queue (audio_command_queue) holds as many (docs/port/AUDIO.md 10.4). Each reader sees
 * the messages in its own view (Msg, Slot18, Slot24, ShutdownMsg, tAudioMsg), the layout below. */
#define PC_SND_N64_MSGS 128
#define PC_SND_MSG_SLOTS 256
int pc_snd_messages(void);
/* In the sound manager's C, where the N64 has a literal n (128) for the messages: the run's, pc_snd_messages(). */
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())
struct pc_snd_msg { short id; signed char type, used; unsigned char payload[16]; void *ent; };
extern struct pc_snd_msg pc_snd_msgs[PC_SND_MSG_SLOTS];   /* N64 audiomgr_gCmds */
extern void *pc_snd_start_cmds[PC_SND_MSG_SLOTS];        /* N64 gStartCmds */
extern void *pc_snd_stop_cmds[PC_SND_MSG_SLOTS];         /* N64 gStopCmds */

/* Counters the game's sound code fills (--log-voices, port/main.c): starts refused or cut and the pools' use. */
enum {
    PC_SND_MX_REFUSED,      /* MusyX found no voice for a sound (sound_alloc_voice: -1) */
    PC_SND_MX_STOLEN,       /* MusyX took a voice from an effect still playing */
    PC_SND_MX_STOLEN_SONG,  /* ... from a song's note still playing (the songs keep 16 voices at any size) */
    PC_SND_FX_REFUSED,      /* an effect the sound manager started that MusyX didn't (AudioPlayEffectInternal) */
    PC_SND_OVER_BUDGET,     /* a tag the priority pass held back or stopped for its budget that would play: playing,
                             * or loud enough to start (_UpdateFx) */
    PC_SND_NO_EMITTER,      /* a positional start that found no emitter (AudEmitEffect) */
    PC_SND_NO_MESSAGE,      /* a start or change that found the message pool empty (GetCmd) */
    PC_SND_NO_ENTRY,        /* an effect start that found no free entry (AudioSfxInit's, AudioPlayEffect) */
    PC_SND_PASSES,          /* the priority passes (_ProcessPriorities): OVER_BUDGET counts tags a pass */
    PC_SND_MX_LOOP_ORPHAN,  /* a synth tick's voice whose macro has ended while its looping sample plays on */
    PC_SND_START_PLAYING,   /* a start the manager's thread dropped: its tag was playing already (audiomgr_thread.c) */
    PC_SND_COUNTERS
};
void pc_snd_count(int what);
void pc_snd_msg_taken(int index);        /* the pool's first-free index: its high-water mark */
enum { PC_SND_MOST_TAGS, PC_SND_MOST_EMITTERS, PC_SND_MOSTS };
void pc_snd_most(int what, int value);   /* a pool's use now: its high-water mark */

/* [audio] engines: which drones' engines play, per listener (PC_ENGINES_*, port/config/voices.c). The emitter pass
 * (_ProcessEmitters, ProcessEmitters.c) notes each drone engine layer's distance to each listener and asks for its
 * gain; the priority pass (_UpdateFx) asks whether a tag is over its budget. */
enum { PC_ENGINES_ORIGINAL = 0, PC_ENGINES_ALL = -1 };   /* else N: the N nearest drones per listener */
int pc_engines(void);
int pc_engines_choosing(void);
void pc_engines_note(int handle, int listener, float distance);
void pc_engines_choose(int listeners);
float pc_engine_gain(int handle, float gain);
void pc_engine_state(int handle, int state);   /* the emitter pass: an emitter's fx tag's state (2: playing) */
void pc_snd_walk_begin(void);
int pc_snd_over_budget(unsigned int tag, int rank, float budget, int audible);

#endif
