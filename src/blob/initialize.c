#include "game/game.h"

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

VIEW(f32[9], unitmatrix);
#define unitmatrix unitmatrix_view

void initialize(Car *car) {
    car->x1004=0.0f;
    car->x952=0.0f; car->x948=0.0f;
    car->x956=50.0f; car->x960=50.0f;
    car->x1588=model_step_dt;
    car->x984=3.0f; car->x1624=0.0f;
    car->x1612=initial_thump_threshold;
    car->x1602=0;
    car->vectors[0][2]=car->vectors[0][1]=car->vectors[0][0]=0.0f;
    car->vectors[1][2]=car->vectors[1][1]=car->vectors[1][0]=0.0f;
    car->vectors[2][2]=car->vectors[2][1]=car->vectors[2][0]=0.0f;
    car->vectors[3][2]=car->vectors[3][1]=car->vectors[3][0]=0.0f;
    car->vectors[5][2]=car->vectors[5][1]=car->vectors[5][0]=0.0f;
    car->vectors[4][2]=car->vectors[4][1]=car->vectors[4][0]=0.0f;
    car->x1032=0.0f;
    CopyUV(unitmatrix,car->matrix);
    if(gSuperSpeed) {
        f32 diagonal=super_speed_unit_diagonal;
        car->matrix[0]=diagonal; car->matrix[4]=diagonal; car->matrix[8]=diagonal;
    }
    carsetup(car);
    init_road(car);
    init_collision(car);
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
