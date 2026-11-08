/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8;typedef unsigned char u8;typedef signed short s16;typedef int s32;typedef unsigned int u32;typedef float f32;
#define FIELD(p,t,o) (*(t *)((u8 *)(p)+(o)))
extern f32 clutch_idle_throttle,clutch_brake_threshold,clutch_rpm_slope;
void SetClutch(void *object) {
 s32 value=FIELD(object,s16,2000);s32 magnitude;
 if(value>=0)magnitude=value;else magnitude=-value;
 if(magnitude<1000 && FIELD(object,f32,1836)<clutch_idle_throttle)FIELD(object,f32,1828)=1.0f;
 else if(FIELD(object,f32,1832)>clutch_brake_threshold) {
 if(magnitude<4000)FIELD(object,f32,1828)=1.0f;
 else if(magnitude>=5000)FIELD(object,f32,1828)=0.0f;
 else FIELD(object,f32,1828)=(5000-magnitude)*clutch_rpm_slope;
 }else FIELD(object,f32,1828)=0.0f;
}
