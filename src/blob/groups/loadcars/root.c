/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef signed int s32;
typedef unsigned int u32;
typedef signed short s16;
typedef signed char s8;
typedef unsigned char u8;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef float f32;
typedef struct Selection8 {u8 reserved;u8 model;s8 type;s8 tune;s8 variant;u8 tail[3];} Selection8;
#ifdef TARGET_PC
enum n64p_void { n64p_void_ };
#endif
#ifdef TARGET_PC
typedef struct Vehicle2056 {enum n64p_void car;enum n64p_void parameters;u8 opaque[1988];s8 mode;u8 tail[59];} Vehicle2056;
#else
typedef struct Vehicle2056 {void *car;void *parameters;u8 opaque[1988];s8 mode;u8 tail[59];} Vehicle2056;
#endif
#ifdef TARGET_PC
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_void: (void *)(uintptr_t)(x), default: (x))
#define N64W(x) PC_N64W(x)
#else
#define N64P(x) x
#define N64W(x) x
#endif
#ifdef TARGET_PC
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Selection8 D_80153E88[13];
#define PC_LINK_CAR(i) D_80153E88[i].model
#define PC_LINK_COLOR0(i) D_80153E88[i].type
#define PC_LINK_COLOR1(i) D_80153E88[i].tune
#define PC_LINK_COLOR2(i) D_80153E88[i].variant
#endif
extern Vehicle2056 D_8014A250[6];
extern u32 D_8002E580[];
extern u32 D_801174B4;
extern s16 D_8014A108;
extern s16 D_80142B08[13];
extern s8 D_80111604[],D_80111648[],D_8011168C[];
typedef struct Heap Heap;
extern u32 audio_output_setup(Heap *);
extern s32 func_80097694(s32,s8);
extern s32 audio_frame_sync(s32,s32,s32,s32,void *);
extern void func_800BB02C(s32,s32,void *);
extern void func_8039156C(void);
extern f32 func_8008B2E4(f32 max);
s32 func_8009768C(s32);
void LoadCars(s32 select)
{
 s32 i;
 s32 j;
 s32 numLoadedCars;
 if (select) {
  func_8039156C();
  return;
 }
 numLoadedCars=0;
 for(i=0;i<13;i++) {
  if(i<6 && ((D_801174B4&8)||D_8014A250[i].mode==1)) {
again:
   PC_LINK_CAR(i)=func_8008B2E4(13.0f);
   for(j=0;j<i;j++) {
    if(PC_LINK_CAR(i)==PC_LINK_CAR(j))goto again;
   }
   PC_LINK_COLOR0(i)=D_80111604[PC_LINK_CAR(i)];
   PC_LINK_COLOR1(i)=D_80111648[PC_LINK_CAR(i)];
   PC_LINK_COLOR2(i)=D_8011168C[PC_LINK_CAR(i)];
   if(audio_output_setup(0)<D_8002E580[PC_LINK_CAR(i)+88]+512) {
    for(j=0;j<13;j++) {
     if(func_8009768C(PC_LINK_CAR(i)+88)<0 &&
        audio_output_setup(0)>=D_8002E580[PC_LINK_CAR(i)+88]+512)goto load;
     PC_LINK_CAR(i)=(PC_LINK_CAR(i)+1)%13;
    }
    for(j=0;j<13;j++) {
     if(audio_output_setup(0)>=D_8002E580[PC_LINK_CAR(i)+88]+512)goto load;
     PC_LINK_CAR(i)=(PC_LINK_CAR(i)+1)%13;
    }
    do { j=func_8008B2E4(numLoadedCars); }
    while(numLoadedCars>D_8014A108 && PC_LINK_CAR(j)>=13);
    PC_LINK_CAR(i)=PC_LINK_CAR(j);
    D_80142B08[i]=D_80142B08[j];
   } else {
load:
    D_80142B08[i]=audio_frame_sync(PC_LINK_CAR(i)+88,1,0,0,0);
    numLoadedCars++;
   }
   func_800BB02C(i,PC_LINK_CAR(i),0);
  } else if(i<6 && D_8014A250[i].mode==2) {
   if(audio_output_setup(0)<D_8002E580[PC_LINK_CAR(i)+88]+512) {
    for(PC_LINK_CAR(i)=0;PC_LINK_CAR(i)<13;PC_LINK_CAR(i)++) {
     if(audio_output_setup(0)>=D_8002E580[PC_LINK_CAR(i)+88]+512)goto load2;
    }
    PC_LINK_CAR(i)=PC_LINK_CAR(0);
    D_80142B08[i]=D_80142B08[0];
    func_800BB02C(i,PC_LINK_CAR(i),0);
   } else {
load2:
    D_80142B08[i]=audio_frame_sync(PC_LINK_CAR(i)+88,1,0,0,0);
    func_800BB02C(i,PC_LINK_CAR(i),0);
    numLoadedCars++;
   }
  } else {
   PC_LINK_CAR(i)=PC_LINK_CAR(0);
   D_80142B08[i]=D_80142B08[0];
  }
 }
}
s32 func_8009768C(s32 waveNum) { return func_80097694(waveNum,-1); }
