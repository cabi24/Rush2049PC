#include "game/game.h"

typedef s16 Position[3];
typedef s16 SearchPoint[4];

/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct CountEntry { u16 count; u8 pad[14]; } CountEntry;

typedef struct Region { u8 pad[40]; s16 path; } Region;
typedef struct Player { u8 pad[0x110 - 0x0]; struct pc_visual visuals[21]; u8 pad_308[0x314 - 0x308]; Region region; u8 rest[0x380 - 0x33E]; void *input; u8 rest_384[0x3B8 - 0x384]; } Player;
typedef struct State { u8 pad[1990]; s16 player; u8 gap[26]; s16 next, previous; } State;

VIEW(CountEntry[], gDronePaths);
#define gDronePaths gDronePaths_view

#define HALF(p,n) (*(s16 *)((u8 *)(p)+(n)))
#define FLOAT(p,n) (*(f32 *)((u8 *)(p)+(n)))

extern f32 sqrtf(f32),fabsf(f32),sinf(f32),cosf(f32);
#pragma intrinsic(sqrtf,fabsf)
typedef struct Descriptor16 {u8 prefix[4];s8 parent;u8 gap;u16 end;u16 gap8;u16 count;Position *points;} Descriptor16;
typedef struct Header16 {u16 count;u16 gap2;Position *points;u8 paths;u8 gap9[3];Descriptor16 *descriptors;} Header16;
typedef struct Route8 {u16 count,gap;SearchPoint *points;} Route8;
typedef struct Record80 {u8 bytes[80];} Record80;
typedef struct Vehicle2056 {u8 prefix[1644];f32 position[3],matrix[9];u8 gap1692[36];f32 speed;u8 gap1732[80];f32 savedSpeed;u8 gap1816[124];f32 savedPosition[3];u8 gap1952[38];s16 player;u8 gap1992[26];s16 current,next;u8 tail[34];} Vehicle2056;
VIEW(Header16, gResPath);
#define gResPath gResPath_view
VIEW(Record80[], gCheckpoints);
#define gCheckpoints gCheckpoints_view
typedef struct Checkpoint80 {f32 pos[3],normal[3];u8 pad18[10];s16 pathPts[20];u8 tail[6];} Checkpoint80;
typedef struct CheckpointHeader {s16 reserved,loopPt;u8 pad4[4];s16 numPoints;u8 padA[2];Checkpoint80 points[1];} CheckpointHeader;
#define CHECKPOINTS (*(CheckpointHeader *)gCheckpoints)

VIEW(f32[9], unitmatrix);
#define unitmatrix unitmatrix_view

typedef struct Surface Surface;
extern Surface *RoadPosition(f32 *,f32 *,f32 *);

void FindSurfaceUvsFromResPoint(s32 pathMode, s32 route, s32 point, f32 *matrix)
{
    float sine, cosine, yaw, angle;
    float snapPos[3], pt[3], pos[3];
    Surface *surf;
    if (route >= 0) {
        if (pathMode) {
            pos[0] = gResPath.descriptors[route].points[point][0];
            pos[1] = gResPath.descriptors[route].points[point][1];
            pos[2] = gResPath.descriptors[route].points[point][2];
            if (point == gResPath.descriptors[route].count - 1) {
                if (gResPath.descriptors[route].parent >= 0) {
                    pt[0] = gResPath.descriptors[gResPath.descriptors[route].parent].points[gResPath.descriptors[route].end][0] - pos[0];
                    pt[1] = gResPath.descriptors[gResPath.descriptors[route].parent].points[gResPath.descriptors[route].end][1] - pos[1];
                    pt[2] = gResPath.descriptors[gResPath.descriptors[route].parent].points[gResPath.descriptors[route].end][2] - pos[2];
                } else {
                    pt[0] = gResPath.points[gResPath.descriptors[route].end][0] - pos[0];
                    pt[1] = gResPath.points[gResPath.descriptors[route].end][1] - pos[1];
                    pt[2] = gResPath.points[gResPath.descriptors[route].end][2] - pos[2];
                }
            } else {
                pt[0] = gResPath.descriptors[route].points[point+1][0] - pos[0];
                pt[1] = gResPath.descriptors[route].points[point+1][1] - pos[1];
                pt[2] = gResPath.descriptors[route].points[point+1][2] - pos[2];
            }
        } else {
            pos[0] = ((Route8 *)gDronePaths)[route].points[point][0];
            pos[1] = ((Route8 *)gDronePaths)[route].points[point][1];
            pos[2] = ((Route8 *)gDronePaths)[route].points[point][2];
            if (point == ((Route8 *)gDronePaths)[route].count - 1) {
                pt[0] = ((Route8 *)gDronePaths)[route].points[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[route+1]][0] - ((Route8 *)gDronePaths)[route].points[point][0];
                pt[1] = ((Route8 *)gDronePaths)[route].points[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[route+1]][1] - ((Route8 *)gDronePaths)[route].points[point][1];
                pt[2] = ((Route8 *)gDronePaths)[route].points[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[route+1]][2] - ((Route8 *)gDronePaths)[route].points[point][2];
            } else {
                pt[0] = ((Route8 *)gDronePaths)[route].points[point+1][0] - ((Route8 *)gDronePaths)[route].points[point][0];
                pt[1] = ((Route8 *)gDronePaths)[route].points[point+1][1] - ((Route8 *)gDronePaths)[route].points[point][1];
                pt[2] = ((Route8 *)gDronePaths)[route].points[point+1][2] - ((Route8 *)gDronePaths)[route].points[point][2];
            }
        }
    } else {
        pos[0] = gResPath.points[point][0];
        pos[1] = gResPath.points[point][1];
        pos[2] = gResPath.points[point][2];
        if (point == gResPath.count - 1) {
            pt[0] = gResPath.points[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[0]][0] - gResPath.points[point][0];
            pt[1] = gResPath.points[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[0]][1] - gResPath.points[point][1];
            pt[2] = gResPath.points[CHECKPOINTS.points[CHECKPOINTS.loopPt].pathPts[0]][2] - gResPath.points[point][2];
        } else {
            pt[0] = gResPath.points[point+1][0] - gResPath.points[point][0];
            pt[1] = gResPath.points[point+1][1] - gResPath.points[point][1];
            pt[2] = gResPath.points[point+1][2] - gResPath.points[point][2];
        }
    }
    normalize_vector_or_x(pt);
    surf = RoadPosition(pos,snapPos,matrix);
    if (surf) {
        angle = fasin(pt[0]);
        if (pt[2] < 0.0f) angle = 3.1415927410125732f - angle;
        ExtractYaw(matrix,&yaw);
        sine = sinf(angle+yaw);
        cosine = cosf(angle+yaw);
    } else {
        sine = pt[1];
        cosine = sqrtf(pt[0]*pt[0] + pt[2]*pt[2]);
        if (pt[2] < 0.0f) sine = -sine;
        matrix[0]=1.0f;
        matrix[5]=-sine;
        matrix[1]=0.0f;
        matrix[2]=0.0f;
        matrix[3]=0.0f;
        matrix[4]=cosine;
        matrix[6]=0.0f;
        matrix[7]=sine;
        matrix[8]=cosine;
        sine=pt[0];
        cosine=pt[2];
    }
    fyaw(-sine,cosine,matrix);
}

void SetResurrectionData(s16 mode,Vehicle2056 *state,u32 callbackFlag) {
    s32 i,index;
    Player *player;
    f32 *dp;
    s32 xv, yv, zv, last_cp_index;
    void *chkpnt;
    switch (mode) {
    case 0:
        if(gGameState&8) {
            FindSurfaceUvsFromResPoint(1,-1,gDemoPathPt,state->matrix);
            for(i=HALF(gCheckpoints,8)-1;i>=0;i--) {
                if(gDemoPathPt>=HALF(&gCheckpoints[i],46)) {
                    state->current=i;
                    if(state->current==HALF(gCheckpoints,8)-1) index=HALF(gCheckpoints,2);
                    else index=state->current+1;
                    state->next=index;
                    break;
                }
            }
            SetDroneResPathIndex((State *)state,-1,gDemoPathPt);
            player=PC_CAR(state->player);
            state->position[0]=(f32)((Route8 *)gDronePaths)[player->region.path].points[HALF(player,824)][0];
            state->position[1]=(f32)((Route8 *)gDronePaths)[player->region.path].points[HALF(player,824)][1];
            state->position[2]=(f32)((Route8 *)gDronePaths)[player->region.path].points[HALF(player,824)][2];
        } else if(gRace==6 || gRace==4) {
#define B135_SELECT(p) ((p)==0?0:((p)==1?2:((p)==2?1:3)))
            state->position[0]=FLOAT(&gCheckpoints[B135_SELECT(state->player)],12);
            state->position[1]=FLOAT(&gCheckpoints[B135_SELECT(state->player)],16);
            state->position[2]=FLOAT(&gCheckpoints[B135_SELECT(state->player)],20);
            CopyUV(unitmatrix,state->matrix);
            state->matrix[6]=FLOAT(&gCheckpoints[B135_SELECT(state->player)],24);
            state->matrix[7]=FLOAT(&gCheckpoints[B135_SELECT(state->player)],28);
            state->matrix[8]=FLOAT(&gCheckpoints[B135_SELECT(state->player)],32);
            state->matrix[7]=0.0f;
            state->matrix[6]*=-1.0f;
            state->matrix[8]*=-1.0f;
            NormalVector(&state->matrix[6]);
            state->matrix[0]=state->matrix[4]*state->matrix[8]-state->matrix[5]*state->matrix[7];
            state->matrix[1]=state->matrix[5]*state->matrix[6]-state->matrix[3]*state->matrix[8];
            state->matrix[2]=state->matrix[3]*state->matrix[7]-state->matrix[4]*state->matrix[6];
            NormalVector(state->matrix);
            SyncMaxpathToLastCheckpoint(state->player);
        } else {
            state->position[0]=(f32)gResPath.points[HALF(&gCheckpoints[state->current],46)][0];
            state->position[1]=(f32)gResPath.points[HALF(&gCheckpoints[state->current],46)][1];
            state->position[2]=(f32)gResPath.points[HALF(&gCheckpoints[state->current],46)][2];
            FindSurfaceUvsFromResPoint(1,-1,HALF(&gCheckpoints[state->current],46),state->matrix);
            SyncMaxpathToLastCheckpoint(state->player);
        }
        state->speed=0.0f;
        break;
    case 2:
        state->position[0]=state->savedPosition[0];
        state->position[1]=state->savedPosition[1];
        state->position[2]=state->savedPosition[2];
        state->speed=state->savedSpeed;
        break;
    }
}
