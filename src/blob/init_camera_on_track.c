/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * init_camera_on_track (0x800EB690, 636 B) -- historical label; really a car respawn-point picker.
 * Walks (speed >> 6) + 4 path points ahead of the object's current point (NextPathPt), takes that
 * path point (gDronePaths[track].points, s16 xyz) as pos, then scans the track's respawn table
 * trackSpots[gTrackNum] (16-byte {s8 flag; f32 pos[3]} records, terminated by pos[1] == -999.0f) for
 * the first record within 200 units in XZ of pos; found -> fix_camera_in_space(flag ? 1 : 0, object,
 * record pos, &matrix). Otherwise lifts pos by 10 + Random(10) into delta, lets CameraCollision(delta,
 * pos) settle it and respawns there with fix_camera_in_space(0, ...). N64 rewrite of the arcade
 * resurrect_car idea (game/sresurrect.c); no line-for-line ancestor.
 * It calls the game's Random and Distance2D, which the N64 build had inlined here as copies (wod_rand,
 * wod_random, wod_dist2d: the same code as game_rand, Random and Distance2D).
 * Shaping (all natural forms): `(s16)(speed >> 6) + 4` (the narrowing as1 deletes pops t6/t7),
 * the point walk on one line (as1 line tie-break), `if (dist > 200.0f) continue;` (numbers the 200.0f
 * web before -999.0f, giving f14/f16), `e` declared before the arrays.
 * Own .rodata: -999.0f (0xC479C000 at 0x80124558).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
typedef struct { u8 other0[248]; s16 speed; u8 other250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 other250_308[0x338 - 0x308]; s16 point; s16 other826; s16 track; } Object;
typedef struct { s16 x, y, z, pad; } PathPoint;
typedef struct { u16 count, other2; PathPoint *points; } Path;
typedef struct { s8 flag; u8 pad1[3]; Vec3 pos; } Respawn;
extern Path gDronePaths[];
extern Respawn *trackSpots[];
extern s8 gTrackNum;
extern s16 NextPathPt(s16, s16);
extern s32 CameraCollision(void *arg0, void *arg1);
extern void fix_camera_in_space(s16 mode, Object *object, f32 *pos, Mat3 *matrix);
extern f32 Distance2D(f32 x, f32 y);   /* sqrtf(x * x + y * y) */
extern f32 Random(f32 max);            /* 0 <= Random(max) < max, from the game's rand */

void init_camera_on_track(Object *object)
{
    s16 i;
    s16 point;
    Respawn *e;
    Vec3 delta;
    Vec3 pos;
    Mat3 matrix;

    i = (s16)(object->speed >> 6) + 4;
    point = object->point;
    for (; i >= 0; i--) point = NextPathPt(point, 0);
    pos[0] = gDronePaths[object->track].points[point].x;
    pos[1] = gDronePaths[object->track].points[point].y;
    pos[2] = gDronePaths[object->track].points[point].z;
    e = trackSpots[gTrackNum];
    if (e != 0) {
        for (; e->pos[1] != -999.0f; e++) {
            delta[0] = e->pos[0] - pos[0];
            delta[1] = e->pos[1] - pos[1];
            delta[2] = e->pos[2] - pos[2];
            if (Distance2D(delta[0], delta[2]) > 200.0f) continue;
            if (e->flag) {
                fix_camera_in_space(1, object, e->pos, &matrix);
            } else {
                fix_camera_in_space(0, object, e->pos, &matrix);
            }
            return;
        }
    }
    delta[0] = pos[0];
    delta[1] = pos[1] + 10.0f + Random(10.0f);
    delta[2] = pos[2];
    CameraCollision(delta, pos);
    fix_camera_in_space(0, object, delta, &matrix);
}
