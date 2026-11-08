/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef float f32;
typedef int s32;
typedef short s16;
typedef unsigned short u16;
typedef unsigned char u8;
typedef struct Position { s16 x,y,z; } Position;
typedef struct SearchPoint { s16 x,y,z,flag; } SearchPoint;
typedef struct PositionEntry { u8 pad[12]; Position *points; } PositionEntry;
/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct CountEntry { u16 count; u8 pad[14]; } CountEntry;
typedef struct PointEntry { SearchPoint *points; u8 pad[4]; } PointEntry;
typedef struct Waypoint { s16 index[40]; } Waypoint;
typedef struct Region { u8 pad[40]; s16 path; } Region;
typedef struct Player { u8 pad[0x110 - 0x0]; struct pc_visual visuals[21]; u8 pad_308[0x314 - 0x308]; Region region; u8 rest[0x380 - 0x33E]; void *input; u8 rest_384[0x3B8 - 0x384]; } Player;
typedef struct State { u8 pad[1990]; s16 player; u8 gap[26]; s16 next, previous; } State;
extern PositionEntry *gResPath_alt;
extern Position *gResPath_pts;
extern CountEntry gDronePaths[];
extern PointEntry gDronePaths_pts[];
extern Waypoint gCheckpoints_pathPts[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern f32 drone_res_path_start_distance;
extern s16 NextPathPt(s16,s16);
extern void SetNewPathIndex(State *,s16);
void SetDroneResPathIndex(State *state, s16 selector, s16 point) {
    f32 x,y,z;
    f32 best;
    f32 dx,dy,dz,distance;
    Position *position;
    SearchPoint *scan;
    Region *region;
    s32 stop;
    s16 nextPoint;
    s32 i;
    best=drone_res_path_start_distance;
    if(selector>=0) {
        position=&gResPath_alt[selector].points[point];
        x=(f32)position->x;
        y=(f32)position->y;
        z=(f32)position->z;
    } else {
        x=(f32)gResPath_pts[point].x;
        y=(f32)gResPath_pts[point].y;
        z=(f32)gResPath_pts[point].z;
    }
    region=&PC_CAR_NAV(state->player);
    if(state->previous<state->next) stop=gDronePaths[region->path].count;
    else stop=gCheckpoints_pathPts[state->previous].index[region->path];
    i=gCheckpoints_pathPts[state->next].index[region->path];
    point=0;
    if(i<stop) {
        scan=&gDronePaths_pts[region->path].points[i];
        do {
            dx=(f32)scan->x-x;
            dy=(f32)scan->y-y;
            dz=(f32)scan->z-z;
            distance=dx*dx+dy*dy+dz*dz;
            if(distance<best) {
                best=distance;
                point=i;
            }
            i++;
            scan++;
        } while(i<stop);
    }
    i=0;
    while(i<state->player) {
        nextPoint=NextPathPt(point,region->path);
        i++;
        point=nextPoint;
    }
    SetNewPathIndex(state,point);
}
