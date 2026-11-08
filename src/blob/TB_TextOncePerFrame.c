/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Quirk: gFrameSeconds is read through a (s32) address cast, which makes IDO take its address in a
 * register (lui/addiu/lwc1 0(reg)) and load it before gColorCycleTime; the two written globals are volatile. */
typedef float f32;
typedef int s32;

extern f32 gFrameSeconds;
extern volatile f32 gColorCycleTime;
extern volatile f32 gColorCycle;

void TB_TextOncePerFrame(void) {
    f32 t;
    f32 u;

    gColorCycleTime += *(f32 *) (intptr_t) &gFrameSeconds;
    t = gColorCycleTime;
    gColorCycle = t + t;
    u = gColorCycle;
    gColorCycle = u - (s32) u;
    u = gColorCycle;
    if (u > 0.5f) {
        gColorCycle = 1.0f - u;
        u = gColorCycle;
    }
    gColorCycle = u + u;
}
