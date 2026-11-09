/* init_cars.c: set the car and model records up for a race (MAT3 mdrive.c, init_cars()). */
#include "game/game.h"

/* A human's input record (gPlayers[i], tPadState) as init_cars reads it: its transmission choice as an unsigned byte,
 * as MAT3's tPadState has it. types.h's tPadState has trans signed, and with it GCC -O0 copies the byte into the model
 * record through another register (the same byte), so this file keeps its own view; the save pointer gives it the
 * record's size and alignment (80 bytes, 8). */
typedef struct PadTrans {
    u8 pad0[0x42];
    u8 trans;                   /* +0x42 bit 0: manual */
    u8 pad43[0x48 - 0x43];
    void *save;                 /* +0x48 */
} PadTrans;

enum n64p_void { n64p_void_ };
/* The model record (tModelData, docs/port/CARS.md 4), the fields init_cars sets. */
typedef struct tModelData {
    enum n64p_void car;         /* +0x000 the physics descriptor's window address */
    u8 tune[4];
    s8 body_type;               /* +0x008 the car type, 0-12 */
    s8 trans;
    s8 autotrans;
    s8 handling;                /* the per-player choices (gCarHandling ... gTireRims) */
    s8 engine_type;
    s8 tire_type;
    s8 frame;
    s8 tireRims;
    u8 pad010[228];
    float bodyR[4][3];          /* +0x0F4 the body's four collision corners, feet */
    u8 pad124[728];
    s32 wingSnd;                /* +0x3FC */
    float catchup;              /* +0x400 */
    u8 pad404[592];
    float colrad;               /* +0x654 the collision radius, feet */
    u8 pad658[108];
    s16 moving_state;           /* +0x6C4 resurrect.moving_state */
    u8 pad6C6[82];
    float dt;                   /* +0x718 fastin.dt: the model step, kept across the reset */
    u8 pad71C[168];
    s16 obResPt;                /* +0x7C4 */
    s16 slot;
    s16 in_game;
    s16 we_control;             /* the computer drives it: a drone of this machine's */
    s8 drone_type;              /* +0x7CC DRONE_TYPE_NONE 0, AI 1, HUMAN 2 */
    s8 color[3];                /* +0x7CD the three paint colours (MAT3 PaintJob) */
    u8 pad7D0[4];
    s32 appearance;             /* +0x7D4 APPEAR_* */
    u8 pad7D8[4];
    s8 started;
    s8 data_valid;
    u8 pad7DE[4];
    s16 last_cp_index;          /* +0x7E2 */
    s16 next_cp_index;
    s16 drone_target;
    u8 pad7E8[2];
    s8 collidable;              /* +0x7EA */
    s8 solid;
    float drone_scale;          /* +0x7EC */
    float time_boost;
    u8 pad7F4[20];
} tModelData;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_void: (void *)PC_N64P(x), default: (x))
/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields init_cars sets. */
typedef struct tCarData {
    u8 pad000[232];
    s32 appearance;             /* +0x0E8 */
    s8 data_valid;
    u8 pad0ED[1];
    s8 place;                   /* +0x0EE */
    u8 pad0EF[17];
    float distance;             /* +0x100 along the track, feet: the grid's order before the start */
    float last_distance;
    u8 pad108[0x110 - 0x108]; struct pc_visual visuals[21]; u8 pad308[0x309 - 0x308];
    s8 solid;                   /* +0x309 */
    u8 pad30A[7];
    s8 collide_state;           /* +0x311 */
    u8 pad312[70];
    s8 crash_state;             /* +0x358 */
    u8 pad359[2];
    s8 slot;                    /* +0x35B */
    s8 camera;                  /* +0x35C the window that follows it, -1 none */
    s8 view;
    s8 saveview;
    u8 pad35F[33];
    PadTrans *player;           /* +0x380 the input record, NULL for a drone (a host pointer) */
    u8 pad384[52];
} tCarData;

/* collsize[body]: the collision box of a car type, feet (MAT3 colfront, colrear, colside, colheight). */
typedef struct Bounds {float front,rear,side,height;} Bounds;

extern PadTrans gPlayers[];

extern float sqrtf(float);
#pragma intrinsic (sqrtf)
/* PC: a race of more cars than the N64's 6 (docs/port/MORECARS.md 3.1). A human's first distance puts it behind
 * every drone: minus the larger of 6 and the race's cars, plus its slot, which is the N64's -(6 - i) in any race of
 * 6 or fewer. The records after the race's cars are reset up to the most a PC race holds, whatever an earlier race
 * of more cars left in them. */
#define GRID_CARS (gNumCars > PC_N64_CARS ? gNumCars : PC_N64_CARS)
#define CLEAR_SLOTS PC_MODEL_SLOTS

/* Reset the race's cars at the race start: clear each model and car record (the model step dt survives), give
 * it the first physics descriptor, the grid order (a human behind the drones), solid and collidable, no window, the
 * views 2, no checkpoint and no respawn; and for each car in the race (the roster's LINK_FLAG_IN_GAME, 0x80) its
 * slot and place, drone or human (owner 6, LINK_OWNER_HUMAN: its input record, and the "drive the hulk" cheat's
 * hulk), its car type, transmission and the per-player choices of handling, engine, tires, frame and rims (row
 * slot + 1 a human's, row 0 the drones', whose engine is slot & 3) and paint, its path, and for a car neither human
 * nor ours (another machine's) the collision box. The rest of the slots are out of the race. In the attract demo
 * the first car is an automatic. Then the drones' schedule (sched_to_use; split screen's from
 * PC_DRONE_SCHED_TWO on). */
void init_cars(void) {
    s32 i,j,index;
    tModelData *m;
    tCarData *car;
    Bounds *sizep;
    float dt,value,maxlen;
    gNumActiveCars=0;
    for(i=0;i<gNumCars;i++) {
        m=PC_MODEL(i);
        car=PC_CAR(i);
        dt=m->dt;
        for(j=0;j<sizeof(tModelData);j++)
            ((s8 *)m)[j]=0;
        m->dt=dt;
        for(j=0;j<sizeof(tCarData);j++)
            ((s8 *)car)[j]=0;
        m->car=PC_N64W(gCarModels[0]);
        m->we_control=PC_LINK_OWNER(i)==0;   /* LINK_OWNER_DRONE */
        car->saveview=2;
        car->camera=-1;
        car->view=car->saveview;
        index=PC_LINK_OWNER(i)==6?GRID_CARS-i:i-gNumPlayers+1;
        car->distance=(float)-index;
        m->solid=1;
        m->collidable=1;
        car->solid=m->solid;
        car->collide_state=-1;
        m->drone_target=0;
        m->drone_scale=1.0f;
        m->time_boost=1.0f;
        car->last_distance=-100.0f;
        m->catchup=1.0f;
        m->obResPt=0;
        m->moving_state=RES_IDLE;
        car->crash_state=CRASH_NONE;
        m->wingSnd=-1;
        InitCarVisuals(car);
        m->in_game=(PC_LINK_FLAGS(i)&0x80)!=0;
        if(m->in_game) {
            PC_MODEL_SLOT(gNumActiveCars++)=i;
            car->slot=i;
            car->place=PC_LINK_PLACE(i);
            m->drone_type=PC_LINK_OWNER(i)<6?1:2;   /* DRONE_TYPE_AI, DRONE_TYPE_HUMAN */
            if(m->drone_type==2) {
                car->player=&gPlayers[i];
                switch(gDriveHulk[i]) {
                case 1:
                    car->appearance=APPEAR_CRASHED;
                    m->appearance=APPEAR_CRASHED;
                    break;
                case 2:
                    car->appearance=APPEAR_HULK;
                    m->appearance=APPEAR_HULK;
                    break;
                }
            }
            if(car->player) {
                m->body_type=PC_LINK_CAR(i);
                m->trans=car->player->trans;
                m->autotrans=(m->trans&1)^1;
                m->handling=gCarHandling[m->slot+1][(u8)m->body_type];
                m->engine_type=gCarEngines[m->slot+1][(u8)m->body_type];
                m->tire_type=gCarTires[m->slot+1][(u8)m->body_type];
                m->frame=gCarFrame[m->slot+1][(u8)m->body_type];
                m->tireRims=TIRE_RIMS(m->slot+1)[(u8)m->body_type];
                m->color[0]=PC_LINK_COLOR0(i);
                m->color[1]=PC_LINK_COLOR1(i);
                m->color[2]=PC_LINK_COLOR2(i);
                m->data_valid=1;
                car->data_valid=1;
            } else if(m->we_control) {
                m->body_type=PC_LINK_CAR(i);
                m->trans=0;
                m->autotrans=1;
                m->handling=gCarHandling[0][(u8)m->body_type];
                m->engine_type=m->slot&3;
                m->tire_type=gCarTires[0][(u8)m->body_type];
                m->frame=gCarFrame[0][(u8)m->body_type];
                m->tireRims=TIRE_RIMS(0)[(u8)m->body_type];
                m->color[0]=PC_LINK_COLOR0(i);
                m->color[1]=PC_LINK_COLOR1(i);
                m->color[2]=PC_LINK_COLOR2(i);
                m->data_valid=1;
                car->data_valid=1;
            }
            m->last_cp_index=0;
            m->next_cp_index=GetNextCheckpoint(m->last_cp_index);
            InitCarPath(m,car);
            if(!car->player&&!m->we_control) {
                sizep=&collsize[(u8)m->body_type];
                value=sizep->side;
                m->bodyR[2][0]=value;
                m->bodyR[0][0]=value;
                value=-sizep->side;
                m->bodyR[3][0]=value;
                m->bodyR[1][0]=value;
                value=sizep->height;
                m->bodyR[1][1]=value;
                m->bodyR[0][1]=value;
                value=sizep->height;
                m->bodyR[3][1]=value;
                m->bodyR[2][1]=value;
                value=sizep->front;
                m->bodyR[1][2]=value;
                m->bodyR[0][2]=value;
                value=-sizep->rear;
                m->bodyR[3][2]=value;
                m->bodyR[2][2]=value;
                maxlen=sizep->front>sizep->rear?sizep->front:sizep->rear;
                m->colrad=sqrtf((maxlen*maxlen+sizep->side*sizep->side)+sizep->height*sizep->height);
            }
        }
    }
    for(i=gNumCars;i<CLEAR_SLOTS;i++) {
        m=PC_MODEL(i);
        m->we_control=PC_LINK_OWNER(i)==0;   /* LINK_OWNER_DRONE */
        m->drone_type=0;   /* DRONE_TYPE_NONE */
        m->in_game=0;
    }
    if(gGameState&GS_DEMO) {
        PC_MODEL_INPUT_FLAGS(0)=0;
        PC_MODEL_AUTOTRANS(0)=1;
    }
    gOneMoreCP=0;
    sched_to_use=num_our_drones;
    if(num_humans>=2) sched_to_use+=PC_DRONE_SCHED_TWO;
    new_sched_to_use=sched_to_use;
    sched_index=-1;
    cur_drone=0;
}
