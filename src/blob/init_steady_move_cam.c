/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed int s32;
typedef struct {
    float f[32];
} Obj;
void steady_move_cam(s32 a, Obj *o, float *pos, float *out);
void init_steady_move_cam(Obj *o) {
    float pos[3];
    float out[9];
    pos[0] = o->f[2] + o->f[17] * 80.0f + o->f[11] * 20.0f;
    pos[1] = o->f[3] + 5.0f;
    pos[2] = o->f[4] + o->f[19] * 80.0f + o->f[13] * 20.0f;
    steady_move_cam(0, o, pos, out);
}
