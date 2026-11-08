/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef signed int s32;
typedef unsigned int u32;
typedef signed short s16;
typedef signed char s8;
typedef unsigned char u8;
typedef float f32;
typedef struct Selection8 {u8 reserved;u8 model;s8 type;s8 tune;s8 variant;u8 tail[3];} Selection8;
enum n64p_void { n64p_void_ };
typedef struct Vehicle2056 {enum n64p_void car;enum n64p_void parameters;u8 opaque[1988];s8 mode;u8 tail[59];} Vehicle2056;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_void: (void *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern u32 gWaveSizes[];
extern u32 gGameState;
extern s16 gNumPlayers;
extern s16 gCarModelNums[13];
extern s8 car_color_choice_a[],car_color_choice_b[],car_color_choice_c[];
typedef struct Heap Heap;
extern u32 heap_total_free(Heap *);
extern s32 FindWaveInstanceInternal(s32,s8);
extern s32 LoadWave(s32,s32,s32,s32,void *);
extern void car_palette_init(s32,s32,void *);
extern void InitSelectCarLoad(void);
extern f32 Random(f32 max);
s32 FindWaveInstance_stub(s32);
/* PC: the heap too short for a car type's file, where LoadCars picks another type or shares one, is logged the first
 * time and counted (port/ultra/heap.c, docs/port/MEMORY.md 5); the choice is the N64's. */

void LoadCars(s32 select)
{
 s32 i;
 s32 j;
 s32 numLoadedCars;
 if (select) {
  InitSelectCarLoad();
  return;
 }
 numLoadedCars=0;
 for(i=0;i<13;i++) {
  if(i<PC_RACE_CARS && ((gGameState&8)||PC_MODEL_DRONE_TYPE(i)==1)) {
again:
   PC_LINK_CAR(i)=Random(13.0f);
   for(j=0;j<i;j++) {
    if(PC_LINK_CAR(i)==PC_LINK_CAR(j))goto again;
   }
   PC_LINK_COLOR0(i)=car_color_choice_a[PC_LINK_CAR(i)];
   PC_LINK_COLOR1(i)=car_color_choice_b[PC_LINK_CAR(i)];
   PC_LINK_COLOR2(i)=car_color_choice_c[PC_LINK_CAR(i)];
   if(heap_total_free(0)<gWaveSizes[PC_LINK_CAR(i)+88]+512) { pc_heap_short("LoadCars, a drone's car", gWaveSizes[PC_LINK_CAR(i)+88]+512);
    for(j=0;j<13;j++) {
     if(FindWaveInstance_stub(PC_LINK_CAR(i)+88)<0 &&
        heap_total_free(0)>=gWaveSizes[PC_LINK_CAR(i)+88]+512)goto load;
     PC_LINK_CAR(i)=(PC_LINK_CAR(i)+1)%13;
    }
    for(j=0;j<13;j++) {
     if(heap_total_free(0)>=gWaveSizes[PC_LINK_CAR(i)+88]+512)goto load;
     PC_LINK_CAR(i)=(PC_LINK_CAR(i)+1)%13;
    }
    do { j=Random(numLoadedCars); }
    while(numLoadedCars>gNumPlayers && PC_LINK_CAR(j)>=13);
    PC_LINK_CAR(i)=PC_LINK_CAR(j);
    gCarModelNums[i]=gCarModelNums[j];
   } else {
load:
    gCarModelNums[i]=LoadWave(PC_LINK_CAR(i)+88,1,0,0,0);
    numLoadedCars++;
   }
   car_palette_init(i,PC_LINK_CAR(i),0);
  } else if(i<PC_RACE_CARS && PC_MODEL_DRONE_TYPE(i)==2) {
   if(heap_total_free(0)<gWaveSizes[PC_LINK_CAR(i)+88]+512) { pc_heap_short("LoadCars, a player's car", gWaveSizes[PC_LINK_CAR(i)+88]+512);
    for(PC_LINK_CAR(i)=0;PC_LINK_CAR(i)<13;PC_LINK_CAR(i)++) {
     if(heap_total_free(0)>=gWaveSizes[PC_LINK_CAR(i)+88]+512)goto load2;
    }
    PC_LINK_CAR(i)=PC_LINK_CAR(0);
    gCarModelNums[i]=gCarModelNums[0];
    car_palette_init(i,PC_LINK_CAR(i),0);
   } else {
load2:
    gCarModelNums[i]=LoadWave(PC_LINK_CAR(i)+88,1,0,0,0);
    car_palette_init(i,PC_LINK_CAR(i),0);
    numLoadedCars++;
   }
  } else {
   PC_LINK_CAR(i)=PC_LINK_CAR(0);
   gCarModelNums[i]=gCarModelNums[0];
  }
 }
}
s32 FindWaveInstance_stub(s32 waveNum) { return FindWaveInstanceInternal(waveNum,-1); }
