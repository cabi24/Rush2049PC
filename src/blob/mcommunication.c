/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef short s16;
typedef struct { char pad[1032]; float f; char pad2[964]; s16 out; } Obj;
extern float rad_per_sec_to_rpm;
extern float mcommunication_rpm_scale;
void mcommunication(Obj *a0) {
    a0->out = a0->f * rad_per_sec_to_rpm * mcommunication_rpm_scale;
}
