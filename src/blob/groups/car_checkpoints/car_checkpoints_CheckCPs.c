/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * CheckCPs -- N64 descendant of arcade CheckCPs() (game/checkpoint.c:866, scp.c:860).
 * For every car (count gNumCars): accumulate the distance driven since last frame into
 * CarData.distance (+0x108) and remember the position (D_80152218[car]); for a live car
 * (player type 0 or 6, not crashed) whose model is not resurrecting (moving == -1), take the
 * side of its next checkpoint's plane; on a side change inside the checkpoint radius report the
 * interpolated crossing time (time - frame_dt * plane / (plane - last_plane)) through
 * PassedCP (arcade PassedCP); keep the plane value per car (D_801527E8).
 * Then, unless gRace == 1: FindMaxpathIntervals (arcade find_maxpath_intervals),
 * PlaceUnlockedCars (the place sort), expire the first-place timer after 3 s, and start the
 * first-place effect (arcade SOUND(S_LEADERLIGHT) = PlayGameSound(16, car, 1, 1), the locked
 * sound wrapper, inlined by umerge).
 *
 * Shaping (each one measured in the whole-program unit):
 *  - the model pointer goes through an integer cast, (Model *)((u32)D_8014A250 + ...): with
 *    &D_8014A250[index] ugen emits `.noalias $4,$sp` and as1 hoists the m->next_cp reloads and the
 *    m->side store across the diff[] stack stores (57 -> 28 words);
 *  - calling PlayGameSound instead of an open-coded `if (gGameSoundsEnabled) AudPlayEffect(...)`:
 *    gives the argument set-up before the test, `li a3,1` and the inverted branch, and also
 *    removes three hoisted invariants from s6-s8 (244 -> 58 words);
 *  - `if (plane < 0) side = -1; else side = 1;` (the empty-else `b`), `diff . normal` in
 *    x, y, z order, gNumCars / gSeconds / gFrameSeconds / num_humans volatile;
 *  - the crossing time goes through one named local assigned twice (cent_dist), which puts
 *    the quotient and the product in $f0;
 *  - frame: arcade declarations reordered; cent_dist and an unused `s16 i` above diff[3],
 *    the scalars between diff[3] and move[3], an unused zvec[3] below (168 bytes, diff at
 *    148, move at 112).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned int u32;
typedef int s32;
typedef float f32;

typedef struct {
    u8 pad0[4];
    f32 time;               /* +0x004 */
    f32 pos[3];             /* +0x008 */
    u8 pad14[0xEE - 0x14];
    s8 blocked;             /* +0x0EE */
    u8 padEF[0x108 - 0xEF];
    f32 distance;           /* +0x108 */
    u8 pad10C[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 pad10C_308[0x358 - 0x308];
    s8 crashed;             /* +0x358 */
    u8 pad359[0x380 - 0x359]; void *input; u8 pad359_384[0x3B8 - 0x384];
} CarData;                  /* 0x3B8 */

typedef struct {
    u8 pad0[0x6C4];
    s16 moving;             /* +0x6C4 */
    u8 pad6C6[0x7E4 - 0x6C6];
    s16 next_cp;            /* +0x7E4 */
    u8 pad7E6[3];
    s8 side;                /* +0x7E9 */
    u8 pad7EA[0x808 - 0x7EA];
} Model;                    /* 0x808 */

typedef struct {
    f32 pos[3];
    f32 normal[3];
    s32 radius;
    u8 pad1C[0x50 - 0x1C];
} CheckPoint;               /* 0x50 */

typedef struct {
    u8 pad0[7];
    u8 type;
} Player;                   /* 8 */

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the cars' checkpoint state is PC data, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 7). */
extern CheckPoint gCheckpoints_points[];
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern volatile s16 gNumCars;
extern s32 gRace;
extern f32 gFirstPlaceTime;
extern volatile s16 num_humans;
extern s8 gInFirstPlace;
extern u8 gPlayers;
extern u32 gGameState;
extern s8 gGameSoundsEnabled;
extern volatile f32 gSeconds;
extern volatile f32 gFrameSeconds;
extern f32 vector_magnitude(f32 *v);
extern void PassedCP(Model *m, f32 t);
extern void FindMaxpathIntervals(void);
extern void PlaceUnlockedCars(void);
extern s32 PlayGameSound(s32 arg0, s32 arg1, s32 arg2, u8 arg3);

void CheckCPs(void)
{
    f32 cent_dist;
    s16 i;
    f32 diff[3];
    s16 index;
    s32 side;
    f32 plane;
    CarData *gc;
    f32 *prev;
    Model *m;
    f32 move[3];
    f32 zvec[3];

    for (index = 0; index < gNumCars; index++) {
        gc = PC_CAR(index);
        prev = PC_CP_LAST_POS(index);
        move[0] = gc->pos[0] - prev[0];
        move[1] = gc->pos[1] - prev[1];
        move[2] = gc->pos[2] - prev[2];
        gc->distance += vector_magnitude(move);
        prev[0] = gc->pos[0];
        prev[1] = gc->pos[1];
        prev[2] = gc->pos[2];
        if ((PC_LINK_OWNER(index) == 0 || PC_LINK_OWNER(index) == 6) && gc->crashed == 0) {
            m = PC_MODEL(index);
            if (m->moving == -1) {
                diff[0] = gc->pos[0] - gCheckpoints_points[m->next_cp].pos[0];
                diff[1] = gc->pos[1] - gCheckpoints_points[m->next_cp].pos[1];
                diff[2] = gc->pos[2] - gCheckpoints_points[m->next_cp].pos[2];
                plane = diff[0] * gCheckpoints_points[m->next_cp].normal[0] + diff[1] * gCheckpoints_points[m->next_cp].normal[1] + diff[2] * gCheckpoints_points[m->next_cp].normal[2];
                if (plane < 0.0f) {
                    side = -1;
                } else {
                    side = 1;
                }
                if (side != m->side) {
                    m->side = side;
                    if (diff[0] * diff[0] + diff[2] * diff[2] < gCheckpoints_points[m->next_cp].radius) {
                        cent_dist = plane / (plane - PC_CP_PLANE(index));
                        cent_dist = gFrameSeconds * cent_dist;
                        PassedCP(m, gc->time - cent_dist);
                    }
                }
                PC_CP_PLANE(index) = plane;
            }
        }
    }
    if (gRace == 1) {
        return;
    }
    FindMaxpathIntervals();
    PlaceUnlockedCars();
    if (gFirstPlaceTime != 0.0f && gSeconds - gFirstPlaceTime > 3.0f) {
        gFirstPlaceTime = 0.0f;
    }
    if (num_humans == 1 && PC_CAR_PLACE(gPlayers) == 0 && !(gGameState & 8)) {
        if (gInFirstPlace == 0 && gFirstPlaceTime == 0.0f) {
            gFirstPlaceTime = gSeconds;
            PlayGameSound(16, gPlayers, 1, 1);
        }
        gInFirstPlace = 1;
    } else {
        gInFirstPlace = 0;
    }
}
