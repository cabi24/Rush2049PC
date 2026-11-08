/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed int s32;
extern float old_drone_scale[];
extern float old_boost[];
void InitDrones(void) {
    s32 i;
    for (i = 0; i < 6; i++) {
        old_drone_scale[i] = 1.0f;
        old_boost[i] = 1.0f;
    }
}
