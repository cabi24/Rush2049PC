/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* point_at_car: follow-camera update for one car slot (N64 UpdateCarObj).
 * Arcade ancestor: game/camera.c UpdateCarObj(). An optional matrix is first
 * forwarded to the car-object setter (update_car_object). The slot's
 * elasticity (elasticity[slot]) is 0 while the car's resurrect state is >= 0,
 * otherwise .6 * the slot's elastic factor (elastic_factor[slot]), further scaled
 * by .75/.6/.5 for player-count modes 3/4/5. The camera position follows
 * carpos + res with that elasticity, and the look vector carpos - campos
 * builds the camera basis (LookInDir -> LookInDir).
 * Shaping: the arcade expression verbatim -- the elasticity is re-read from
 * the global in both factors (no weight/inverse locals) and the sum is
 * written (carpos[i]+res[i]). A named `inverse` local is what flipped the
 * mul.s operand order in every earlier attempt. Own literal 0.6f (0x3F19999A)
 * is own .rodata, verified by the scorer.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
typedef f32 Mat3[3][3];
typedef struct { u8 other[0x110 - 0x0]; struct pc_visual visuals[21]; u8 other_308[0x35B - 0x308]; s8 car,slot; } Object;
typedef struct { s16 status;u8 other[2054]; } CarField2056;
typedef struct { u8 other[96];Mat3 matrix;f32 position[3];u8 tail[8]; } Slot152;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern f32 elasticity[],elastic_factor[];
extern volatile s32 gLastFrameCount;
extern Slot152 gCamera[];
extern void update_car_object(void *,void *,void *);
extern void LookInDir(f32 *,f32 [3][3]);
/* game.frame_rate 60: the elasticity is per frame of two retraces, e; a frame of one gets e^(1/2), so two of them
 * follow the car as one of two retraces does (docs/port/FRAMERATE.md 7). The N64's own scaling stays for 3-5. */
#define PC_CAM_ELASTIC(e) (pc_fps60 && gLastFrameCount == 1 ? __builtin_sqrtf(e) : (e))
void point_at_car(Object *object,f32 *carpos,Mat3 *matrix,f32 *res)
{
    s32 i;
    f32 rpos[3];
    s32 slot=object->slot;
    if(matrix) update_car_object(object,carpos,matrix);
    if(PC_MODEL_MOVING_STATE(object->car)>=0) {
        elasticity[slot]=0.0f;
    } else {
        elasticity[slot]=PC_CAM_ELASTIC(elastic_factor[slot]*0.6f);
        if(gLastFrameCount==3) elasticity[slot]*=0.75f;
        else if(gLastFrameCount==4) elasticity[slot]*=0.6f;
        else if(gLastFrameCount==5) elasticity[slot]*=0.5f;
    }
    for(i=0;i<3;i++) {
        gCamera[slot].position[i]=(gCamera[slot].position[i]*elasticity[slot]+(carpos[i]+res[i])*(1.0f-elasticity[slot]));
        rpos[i]=carpos[i]-gCamera[slot].position[i];
    }
    LookInDir(rpos,gCamera[slot].matrix);
}
