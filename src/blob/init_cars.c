/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * init_cars @ 0x800EC2F8, 1,564 bytes (from best.c, 60 aligned rows / +3 words).
 * Names are historical labels.
 *
 * The N64 descendant of arcade init_cars() (game/mdrive.c): for each of gNumCars players clear model[i]
 * (D_8014A250, 2056 bytes, preserving the f32 at +1816) and game_car[i] (gGameCar, 952 bytes) byte by byte,
 * initialise them from the link record gLink[i] (D_80153E88, 8 bytes: body +1, flags +6, owner +7), call
 * InitCarVisuals(gc); in-game cars get model[num_active++].slot = i (num_active = gNumActiveCars), handicap bytes
 * from five s8[][13] tables indexed [slot+1][body] (or [0][body]), next checkpoint = GetNextCheckpoint
 * (arcade get_next_checkpoint, locked; defined here only so umerge can inline it, as it does in the
 * whole-program unit), InitCarPath(m, gc), and for remote cars the arcade BODYR/colrad block from
 * collsize collsize[body]. Then cars i >= count are reset and a few globals set.
 *
 * Shaping this match depends on (levers plus):
 *  - gLink[i] is indexed directly (D_80153E88[i].owner etc.) as in the arcade, not through a pointer local:
 *    with a `config` pointer as1 has no .noalias for it against `car` and keeps the stores before the loads;
 *  - `owner == 6` (int literal) and the index computed as one `?:`; an if/else over two lines leaves
 *    as1's hoisted `li 6` after the `lui` (the one-line `if (..) a; else b;` also matches: .loc layout);
 *  - each byte-clear loop has its body on its own line (`for (...)\n    ((s8 *)m)[j] = 0;`, as the arcade
 *    writes it): with header and body on one line as1 fills the delay slot with the pointer increment;
 *  - D_8014A250 is *defined* (tentative definition, as arcade globals.h does for model[]) so as1 shares one
 *    `lui at` for the two stores to model[0] (precedent: ClearControls);
 *  - volatile s16 gNumCars / sched_to_use / num_humans, switch for gDriveHulk[i], (u8) re-read of byte8,
 * statement order; #pragma intrinsic (sqrtf).
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned int u32;
typedef int s32;
typedef struct Reference {
    u32 word0;
    u8 opaque4[62];
    u8 flags;
    u8 opaque67[0x48 - 0x43]; void *ref;
} Reference;
enum n64p_void { n64p_void_ };
typedef struct Car {
    enum n64p_void object;
    u8 opaque4[4];
    s8 byte8;
    s8 byte9;
    s8 byte10;
    s8 byte11;
    s8 byte12;
    s8 byte13;
    s8 byte14;
    s8 byte15;
    u8 opaque16[228];
    float value244;
    float value248;
    float value252;
    float value256;
    float value260;
    float value264;
    float value268;
    float value272;
    float value276;
    float value280;
    float value284;
    float value288;
    u8 opaque292[728];
    s32 word1020;
    float value1024;
    u8 opaque1028[592];
    float radius;
    u8 opaque1624[108];
    s16 half1732;
    u8 opaque1734[82];
    float preserved;
    u8 opaque1820[168];
    s16 half1988;
    s16 half1990;
    s16 half1992;
    s16 half1994;
    s8 byte1996;
    s8 byte1997;
    s8 byte1998;
    s8 byte1999;
    u8 opaque2000[4];
    s32 word2004;
    u8 opaque2008[4];
    s8 byte2012;
    s8 byte2013;
    u8 opaque2014[4];
    s16 half2018;
    s16 half2020;
    s16 half2022;
    u8 opaque2024[2];
    s8 byte2026;
    s8 byte2027;
    float scale;
    float time_boost;
    u8 opaque2036[20];
} Car;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_void: (void *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
typedef struct Player {
    u8 opaque0[232];
    s32 word232;
    s8 byte236;
    u8 opaque237[1];
    s8 byte238;
    u8 opaque239[17];
    float distance;
    float last_distance;
    u8 opaque264[0x110 - 0x108]; struct pc_visual visuals[21]; u8 opaque264_308[0x309 - 0x308];
    s8 byte777;
    u8 opaque778[7];
    s8 byte785;
    u8 opaque786[70];
    s8 byte856;
    u8 opaque857[2];
    s8 byte859;
    s8 byte860;
    s8 byte861;
    s8 byte862;
    u8 opaque863[33];
    Reference * reference;
    u8 opaque900[52];
} Player;
typedef struct Config {u8 field0,body,field2,field3,field4,place,flags,owner;} Config;
typedef struct Bounds {float front,rear,side,height;} Bounds;
typedef struct Route {s16 field0,first,field4,field6,count;} Route;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Reference gPlayers[];
extern Bounds collsize[];
extern Route gCheckpoints;
extern s8 gDriveHulk[];
extern s8 gCarHandling[][13],gCarEngines[][13],gCarTires[][13],gCarFrame[][13],gTireRims[][13];
extern s8 gNumActiveCars;
extern s8 gOneMoreCP;
extern volatile s16 gNumCars;
extern volatile s16 sched_to_use;
extern volatile s16 num_humans;
extern s16 gNumPlayers,num_our_drones,new_sched_to_use,sched_index,cur_drone;
extern void *gCarModels;
extern s32 gGameState;
extern void InitCarVisuals(Player *);
extern void InitCarPath(Car *,Player *);
extern float sqrtf(float);
#pragma intrinsic (sqrtf)
/* PC: a race of more cars than the N64's 6 (docs/port/MORECARS.md 3.1). A human's first distance puts it behind
 * every drone: minus the larger of 6 and the race's cars, plus its slot, which is the N64's -(6 - i) in any race of
 * 6 or fewer. The records after the race's cars are reset up to the most a PC race holds, whatever an earlier race
 * of more cars left in them. */
#define GRID_CARS (gNumCars > PC_N64_CARS ? gNumCars : PC_N64_CARS)
#define CLEAR_SLOTS PC_MODEL_SLOTS
s32 GetNextCheckpoint(s32 index);
void init_cars(void) {
    s32 i,j,index;
    Car *car;
    Player *player;
    Bounds *bounds;
    float preserved,value,maxlen;
    gNumActiveCars=0;
    for(i=0;i<gNumCars;i++) {
        car=PC_MODEL(i);
        player=PC_CAR(i);
        preserved=car->preserved;
        for(j=0;j<sizeof(Car);j++)
            ((s8 *)car)[j]=0;
        car->preserved=preserved;
        for(j=0;j<sizeof(Player);j++)
            ((s8 *)player)[j]=0;
        car->object=N64W(gCarModels);
        car->half1994=PC_LINK_OWNER(i)==0;
        player->byte862=2;
        player->byte860=-1;
        player->byte861=player->byte862;
        index=PC_LINK_OWNER(i)==6?GRID_CARS-i:i-gNumPlayers+1;
        player->distance=(float)-index;
        car->byte2027=1;
        car->byte2026=1;
        player->byte777=car->byte2027;
        player->byte785=-1;
        car->half2022=0;
        car->scale=1.0f;
        car->time_boost=1.0f;
        player->last_distance=-100.0f;
        car->value1024=1.0f;
        car->half1988=0;
        car->half1732=-1;
        player->byte856=0;
        car->word1020=-1;
        InitCarVisuals(player);
        car->half1992=(PC_LINK_FLAGS(i)&0x80)!=0;
        if(car->half1992) {
            PC_MODEL_SLOT(gNumActiveCars++)=i;
            player->byte859=i;
            player->byte238=PC_LINK_PLACE(i);
            car->byte1996=PC_LINK_OWNER(i)<6?1:2;
            if(car->byte1996==2) {
                player->reference=&gPlayers[i];
                switch(gDriveHulk[i]) {
                case 1:
                    player->word232=112;
                    car->word2004=112;
                    break;
                case 2:
                    player->word232=16;
                    car->word2004=16;
                    break;
                }
            }
            if(player->reference) {
                car->byte8=PC_LINK_CAR(i);
                car->byte9=player->reference->flags;
                car->byte10=(car->byte9&1)^1;
                car->byte11=gCarHandling[car->half1990+1][(u8)car->byte8];
                car->byte12=gCarEngines[car->half1990+1][(u8)car->byte8];
                car->byte13=gCarTires[car->half1990+1][(u8)car->byte8];
                car->byte14=gCarFrame[car->half1990+1][(u8)car->byte8];
                car->byte15=gTireRims[car->half1990+1][(u8)car->byte8];
                car->byte1997=PC_LINK_COLOR0(i);
                car->byte1998=PC_LINK_COLOR1(i);
                car->byte1999=PC_LINK_COLOR2(i);
                car->byte2013=1;
                player->byte236=1;
            } else if(car->half1994) {
                car->byte8=PC_LINK_CAR(i);
                car->byte9=0;
                car->byte10=1;
                car->byte11=gCarHandling[0][(u8)car->byte8];
                car->byte12=car->half1990&3;
                car->byte13=gCarTires[0][(u8)car->byte8];
                car->byte14=gCarFrame[0][(u8)car->byte8];
                car->byte15=gTireRims[0][(u8)car->byte8];
                car->byte1997=PC_LINK_COLOR0(i);
                car->byte1998=PC_LINK_COLOR1(i);
                car->byte1999=PC_LINK_COLOR2(i);
                car->byte2013=1;
                player->byte236=1;
            }
            car->half2018=0;
            car->half2020=GetNextCheckpoint(car->half2018);
            InitCarPath(car,player);
            if(!player->reference&&!car->half1994) {
                bounds=&collsize[(u8)car->byte8];
                value=bounds->side;
                car->value268=value;
                car->value244=value;
                value=-bounds->side;
                car->value280=value;
                car->value256=value;
                value=bounds->height;
                car->value260=value;
                car->value248=value;
                value=bounds->height;
                car->value284=value;
                car->value272=value;
                value=bounds->front;
                car->value264=value;
                car->value252=value;
                value=-bounds->rear;
                car->value288=value;
                car->value276=value;
                maxlen=bounds->front>bounds->rear?bounds->front:bounds->rear;
                car->radius=sqrtf((maxlen*maxlen+bounds->side*bounds->side)+bounds->height*bounds->height);
            }
        }
    }
    for(i=gNumCars;i<CLEAR_SLOTS;i++) {
        car=PC_MODEL(i);
        car->half1994=PC_LINK_OWNER(i)==0;
        car->byte1996=0;
        car->half1992=0;
    }
    if(gGameState&8) {
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
