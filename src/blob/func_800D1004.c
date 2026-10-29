/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned int u32;
typedef float f32;
typedef struct Car {
    u8 opaque0[16]; f32 vectors[6][3];
    u8 opaque88[660]; f32 matrix[9];
    u8 opaque784[164]; f32 x948, x952, x956, x960;
    u8 opaque964[20]; f32 x984;
    u8 opaque988[16]; f32 x1004;
    u8 opaque1008[24]; f32 x1032;
    u8 opaque1036[552]; f32 x1588;
    u8 opaque1592[8]; u8 x1600, x1601, x1602;
    u8 opaque1603[9]; f32 x1612, x1616;
    u8 opaque1620[4]; f32 x1624; s16 x1628;
    u8 opaque1630[360]; s16 index1990;
} Car;
typedef struct Player952 {
#ifdef TARGET_PC
    u8 opaque0[0x110 - 0x0]; struct pc_visual visuals[21]; u8 opaque0_308[0x358 - 0x308]; u8 x856;
#else
    u8 opaque0[856]; u8 x856;
#endif
#ifdef TARGET_PC
    u8 opaque857[0x380 - 0x359]; void *input; u8 opaque857_384[0x386 - 0x384]; s16 x902;
#else
    u8 opaque857[45]; s16 x902;
#endif
    u8 opaque904[4]; u32 x908;
    u8 opaque912[16]; u8 x928, x929, x930;
    u8 opaque931[13]; f32 x944, x948;
} Player952;
#ifdef TARGET_PC
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Player952 player_array[];
#define PC_CAR_B358(i) player_array[i].x856
#define PC_CAR_B3A0(i) player_array[i].x928
#define PC_CAR_B3A2(i) player_array[i].x930
#define PC_CAR_BATTLE_FLAGS(i) player_array[i].x908
#define PC_CAR_F3B0(i) player_array[i].x944
#define PC_CAR_F3B4(i) player_array[i].x948
#define PC_CAR_HEALTH(i) player_array[i].x902
#endif
extern f32 D_8011418C[9];
extern f32 D_80124160, D_80124164, D_80124168;
extern s8 D_801427A1;
extern void math_utility(f32 *,f32 *);
extern void track_info_display(Car *);
extern void menu_control_settings(Car *);
extern void func_800D0810(Car *);
void func_800D1004(Car *car) {
    car->x1004=0.0f;
    car->x952=0.0f; car->x948=0.0f;
    car->x956=50.0f; car->x960=50.0f;
    car->x1588=D_80124160;
    car->x984=3.0f; car->x1624=0.0f;
    car->x1612=D_80124164;
    car->x1602=0;
    car->vectors[0][2]=car->vectors[0][1]=car->vectors[0][0]=0.0f;
    car->vectors[1][2]=car->vectors[1][1]=car->vectors[1][0]=0.0f;
    car->vectors[2][2]=car->vectors[2][1]=car->vectors[2][0]=0.0f;
    car->vectors[3][2]=car->vectors[3][1]=car->vectors[3][0]=0.0f;
    car->vectors[5][2]=car->vectors[5][1]=car->vectors[5][0]=0.0f;
    car->vectors[4][2]=car->vectors[4][1]=car->vectors[4][0]=0.0f;
    car->x1032=0.0f;
    math_utility(D_8011418C,car->matrix);
    if(D_801427A1) {
        f32 diagonal=D_80124168;
        car->matrix[0]=diagonal; car->matrix[4]=diagonal; car->matrix[8]=diagonal;
    }
    track_info_display(car);
    menu_control_settings(car);
    func_800D0810(car);
    car->x1600=0;
    PC_CAR_HEALTH(car->index1990)=800;
    PC_CAR_B3A0(car->index1990)=0;
    PC_CAR_B3A2(car->index1990)=0;
    PC_CAR_BATTLE_FLAGS(car->index1990)=0;
    PC_CAR_F3B0(car->index1990)=0.0f;
    PC_CAR_F3B4(car->index1990)=0.0f;
    PC_CAR_B358(car->index1990)=0;
    car->x1616=0.0f;
    car->x1628=3;
}
