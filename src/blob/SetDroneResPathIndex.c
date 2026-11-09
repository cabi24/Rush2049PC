#include "game/game.h"

typedef struct Position { s16 x,y,z; } Position;
typedef struct SearchPoint { s16 x,y,z,flag; } SearchPoint;
typedef struct PositionEntry { u8 pad[12]; Position *points; } PositionEntry;
/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct CountEntry { u16 count; u8 pad[14]; } CountEntry;
/* gDronePaths' elements as this code reads them: the point count and the points. */
typedef struct DronePath { u16 count; SearchPoint *points; } DronePath;
typedef struct Region { u8 pad[40]; s16 path; } Region;

typedef struct State { u8 pad[1990]; s16 player; u8 gap[26]; s16 next, previous; } State;
/* The track's path (FindResPtRecurse.c's PathGraph): its points, and its alternative paths (nodes). */
typedef struct { u16 numPoints; Position *points; u8 numNodes; PositionEntry *nodes; } PathGraph;
VIEW(PathGraph, gResPath);
#define gResPath gResPath_view
VIEW(CountEntry[], gDronePaths);
#define gDronePaths gDronePaths_view

void SetDroneResPathIndex(State *state, s16 selector, s16 point) {
    DronePath *drone_paths = (DronePath *) gDronePaths;
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
        position=&gResPath.nodes[selector].points[point];
        x=(f32)position->x;
        y=(f32)position->y;
        z=(f32)position->z;
    } else {
        x=(f32)gResPath.points[point].x;
        y=(f32)gResPath.points[point].y;
        z=(f32)gResPath.points[point].z;
    }
    region=&PC_CAR_NAV(state->player);
    if(state->previous<state->next) stop=gDronePaths[region->path].count;
    else stop=gCheckpoints.points[state->previous].pathPts[region->path + 1];
    i=gCheckpoints.points[state->next].pathPts[region->path + 1];
    point=0;
    if(i<stop) {
        scan=&drone_paths[region->path].points[i];
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
