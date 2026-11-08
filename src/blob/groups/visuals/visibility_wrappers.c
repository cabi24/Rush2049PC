/* Genuine complete logical bodies, reconstructed from the native
 * 18/25-word routines. Private native register conventions require actual
 * source context; this file makes no ordinary singleton match claim. */
#ifndef VISUAL_TIRE_TYPES_H
#define VISUAL_TIRE_TYPES_H
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;
typedef f32 Vec3[3];
typedef f32 Matrix3[3][3];
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct Visual {
    void *prefix;
    s16 tire,object,slot,reserved;
    f32 angle,time;
    void (*callback)(void);
} Visual;
typedef struct Tire92 {
    u8 prefix[64];
    f32 angular_velocity;
    u8 tail[24];
} Tire92;
typedef struct Model2056 {
    u8 prefix[8];
    u8 car_type;
    u8 to_body_type[6];
    s8 body_type;
    u8 to_steering[928];
    f32 steering;
    u8 to_tires[132];
    Tire92 tires[4];
    u8 to_suspension[36];
    f32 suspension[4];
    u8 to_visual_code[64];
    u16 visual_code[4];
    u8 to_player[418];
    s16 player;
    u8 to_mode[4];
    s8 mode;
    u8 tail[59];
} Model2056;
typedef struct Car952 {
    u8 prefix[232];
    u32 appearance;
    u8 to_mph[12];
    s16 mph;
    u8 to_controller[610];
    s8 controller,render_state,reserved,hidden;
    u8 tail[88];
} Car952;
enum n64p_f32 { n64p_f32_ };
typedef struct CarConfig {
    u8 prefix[96];
    enum n64p_f32 radius[4];
    Vec3 tire_position[4];
} CarConfig;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_f32: (f32 *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
typedef struct Control152 {
    u8 prefix[144];
    f32 spring_save;
    u8 tail[4];
} Control152;
typedef struct Object68 {
    u32 flags;
    u8 tail[64];
} Object68;
extern Model2056 D_8014A250[];
extern Car952 gGameCar[];
extern CarConfig *gCarModels[];
extern Control152 gCamera[];
extern Object68 ObjInstanceList[];
extern s8 gInvisibleCar,expansion_pak_present,D_8015978C;
extern u8 tire_blur_texture[];
extern s32 gWheelTextures[];
extern f32 gFrontTireScale[][13],gRearTireScale[][13];
extern f32 gModelTime;
extern f32 D_8012393C,D_80123940,D_80123944,D_80123948;
extern s32 game_rand(void);
extern void MBOX_HideObject(s16,s32,s32);
extern void MBOX_ShowObject(s16,s32,s32);
extern s32 CheckVisibleAll(Visual *,s32);
extern s32 CheckVisible(Visual *,s32,s32);
extern void ZOID_SetTexture(s16,s32,s32);
extern void CreateYPRMatrix(Matrix3,Vec3);
extern void ScaleUV(Matrix3,Matrix3,f32);
extern void ZOID_UpdateObject(s16,Vec3,Matrix3);
#endif

s32 CheckVisibleAll(Visual *v,s32 show);
s32 CheckVisible(Visual *v,s32 show,s32 part)
{
    if(part<0) return CheckVisibleAll(v,show);
    if(show) MBOX_ShowObject(v->object,0,1<<part);
    else MBOX_HideObject(v->object,1,1<<part);
    return show;
}
