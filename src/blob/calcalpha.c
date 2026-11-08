/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef float f32;
extern f32 calcalpha_far_scale_positive,calcalpha_far_scale_negative;
f32 calcalpha(f32 *vector) {
    f32 value;
    f32 result;
    f32 z;
    value=vector[0];
    result=value;
    z=vector[2];
    if(z>0.0f) {
        if(z<400.0f) result=value/z;
        else result=value*calcalpha_far_scale_positive;
    } else if(z<0.0f) {
        if(z>-400.0f) result=(-value)/z;
        else result=value*calcalpha_far_scale_negative;
    }
    return result;
}
